#include "aoe/multiplayer_transport.hpp"

#include <algorithm>
#include <cerrno>
#include <limits>
#include <stdexcept>
#include <string>

#if defined(AOE_NO_NATIVE_TCP)
#elif defined(_WIN32)
#error "TcpFrameStream requires a Windows socket implementation"
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <unistd.h>
#endif

namespace aoe {
namespace {

#if !defined(AOE_NO_NATIVE_TCP)
void close_descriptor(int& descriptor) {
    if (descriptor >= 0) {
        ::shutdown(descriptor, SHUT_RDWR);
        ::close(descriptor);
        descriptor = -1;
    }
}

std::runtime_error socket_error(const std::string& operation) {
    return std::runtime_error(
        operation + " failed with errno " + std::to_string(errno)
    );
}

void configure_stream_descriptor(int descriptor) {
#if defined(__APPLE__) && defined(SO_NOSIGPIPE)
    int enabled = 1;
    if (::setsockopt(
            descriptor, SOL_SOCKET, SO_NOSIGPIPE,
            &enabled, sizeof(enabled)
        ) != 0) {
        throw socket_error("setsockopt SO_NOSIGPIPE");
    }
#else
    (void)descriptor;
#endif
}
#endif

bool valid_utf8(const std::string& text) {
    std::size_t index{};
    while (index < text.size()) {
        const unsigned char lead = text[index++];
        if (lead < 0x80) {
            if (lead == 0) return false;
            continue;
        }
        int continuation{};
        std::uint32_t codepoint{};
        if ((lead & 0xe0) == 0xc0) {
            continuation = 1;
            codepoint = lead & 0x1f;
        } else if ((lead & 0xf0) == 0xe0) {
            continuation = 2;
            codepoint = lead & 0x0f;
        } else if ((lead & 0xf8) == 0xf0) {
            continuation = 3;
            codepoint = lead & 0x07;
        } else {
            return false;
        }
        if (index + continuation > text.size()) return false;
        for (int count = 0; count < continuation; ++count) {
            const unsigned char byte = text[index++];
            if ((byte & 0xc0) != 0x80) return false;
            codepoint = (codepoint << 6) | (byte & 0x3f);
        }
        if ((continuation == 1 && codepoint < 0x80) ||
            (continuation == 2 && codepoint < 0x800) ||
            (continuation == 3 && codepoint < 0x10000) ||
            codepoint > 0x10ffff ||
            (codepoint >= 0xd800 && codepoint <= 0xdfff)) {
            return false;
        }
    }
    return true;
}

}  // namespace

LatencyBand latency_band_for_rtt(std::uint64_t milliseconds) {
    if (milliseconds < 300) return LatencyBand::green;
    if (milliseconds <= 1000) return LatencyBand::yellow;
    return LatencyBand::red;
}

#if defined(AOE_NO_NATIVE_TCP)

namespace {
[[noreturn]] void browser_transport_unavailable() {
    throw std::runtime_error(
        "native TCP multiplayer is unavailable in frontend-only browser build"
    );
}
}  // namespace

TcpFrameStream::~TcpFrameStream() = default;
TcpFrameStream::TcpFrameStream(TcpFrameStream&&) noexcept = default;
TcpFrameStream& TcpFrameStream::operator=(TcpFrameStream&&) noexcept = default;
void TcpFrameStream::send_frame(const LockstepFrame&) {
    browser_transport_unavailable();
}
void TcpFrameStream::send_frame_fragmented(const LockstepFrame&, std::size_t) {
    browser_transport_unavailable();
}
std::optional<LockstepFrame> TcpFrameStream::receive_frame() {
    browser_transport_unavailable();
}
void TcpFrameStream::set_nonblocking() { browser_transport_unavailable(); }
void TcpFrameStream::queue_frame(const LockstepFrame&) {
    browser_transport_unavailable();
}
bool TcpFrameStream::flush_queued() { browser_transport_unavailable(); }
TcpFramePoll TcpFrameStream::poll_frame() { browser_transport_unavailable(); }
TcpConnectStatus TcpFrameStream::connect_status() const {
    return TcpConnectStatus::failed;
}
void TcpFrameStream::close() { descriptor_ = -1; }
void TcpFrameStream::send_bytes(const std::string&, std::size_t) {
    browser_transport_unavailable();
}

TcpFrameListener::TcpFrameListener(std::uint16_t) {
    browser_transport_unavailable();
}
TcpFrameListener::~TcpFrameListener() = default;
TcpFrameListener::TcpFrameListener(TcpFrameListener&&) noexcept = default;
TcpFrameListener& TcpFrameListener::operator=(TcpFrameListener&&) noexcept =
    default;
TcpFrameStream TcpFrameListener::accept() { browser_transport_unavailable(); }
void TcpFrameListener::set_nonblocking() { browser_transport_unavailable(); }
std::optional<TcpFrameStream> TcpFrameListener::try_accept() {
    browser_transport_unavailable();
}
TcpFrameStream connect_localhost(std::uint16_t) {
    browser_transport_unavailable();
}
std::optional<TcpFrameStream> try_connect_localhost(std::uint16_t) {
    browser_transport_unavailable();
}
TcpFrameStream begin_connect_localhost(std::uint16_t) {
    browser_transport_unavailable();
}

#else

TcpFrameStream::~TcpFrameStream() {
    close();
}

TcpFrameStream::TcpFrameStream(TcpFrameStream&& other) noexcept
    : descriptor_(other.descriptor_),
      nonblocking_(other.nonblocking_),
      receive_buffer_(std::move(other.receive_buffer_)),
      send_buffer_(std::move(other.send_buffer_)) {
    other.descriptor_ = -1;
}

TcpFrameStream& TcpFrameStream::operator=(TcpFrameStream&& other) noexcept {
    if (this != &other) {
        close();
        descriptor_ = other.descriptor_;
        nonblocking_ = other.nonblocking_;
        receive_buffer_ = std::move(other.receive_buffer_);
        send_buffer_ = std::move(other.send_buffer_);
        other.descriptor_ = -1;
    }
    return *this;
}

void TcpFrameStream::close() {
    close_descriptor(descriptor_);
}

void TcpFrameStream::send_bytes(
    const std::string& bytes,
    std::size_t fragment
) {
    if (!open() || fragment == 0) {
        throw std::invalid_argument("invalid TCP frame stream send");
    }
    std::size_t offset = 0;
    while (offset < bytes.size()) {
        const std::size_t count = std::min(fragment, bytes.size() - offset);
        const auto sent = ::send(
            descriptor_,
            bytes.data() + offset,
            count,
#if defined(MSG_NOSIGNAL)
            MSG_NOSIGNAL
#else
            0
#endif
        );
        if (sent < 0) {
            if (errno == EINTR) continue;
            throw socket_error("send");
        }
        if (sent == 0) throw std::runtime_error("TCP peer closed during send");
        offset += static_cast<std::size_t>(sent);
    }
}

void TcpFrameStream::send_frame(const LockstepFrame& frame) {
    if (nonblocking_) {
        queue_frame(frame);
        (void)flush_queued();
        return;
    }
    const std::string bytes = encode_lockstep_frame(frame);
    send_bytes(bytes, bytes.size());
}

void TcpFrameStream::set_nonblocking() {
    if (!open()) throw std::runtime_error("closed TCP frame stream");
    const int flags = ::fcntl(descriptor_, F_GETFL, 0);
    if (flags < 0 ||
        ::fcntl(descriptor_, F_SETFL, flags | O_NONBLOCK) != 0) {
        throw socket_error("fcntl nonblocking");
    }
    nonblocking_ = true;
}

void TcpFrameStream::queue_frame(const LockstepFrame& frame) {
    send_buffer_ += encode_lockstep_frame(frame);
    if (send_buffer_.size() > 4 * 1024 * 1024) {
        throw std::runtime_error("TCP frame send queue exceeded");
    }
}

bool TcpFrameStream::flush_queued() {
    while (!send_buffer_.empty()) {
        const auto sent = ::send(
            descriptor_,
            send_buffer_.data(),
            send_buffer_.size(),
#if defined(MSG_NOSIGNAL)
            MSG_NOSIGNAL
#else
            0
#endif
        );
        if (sent < 0) {
            if (errno == EINTR) continue;
            if (errno == EAGAIN || errno == EWOULDBLOCK) return false;
            throw socket_error("nonblocking send");
        }
        if (sent == 0) return false;
        send_buffer_.erase(0, static_cast<std::size_t>(sent));
    }
    return true;
}

TcpFramePoll TcpFrameStream::poll_frame() {
    if (!open()) return {TcpPollStatus::disconnected, std::nullopt};
    while (true) {
        const auto separator = receive_buffer_.find(':');
        if (separator != std::string::npos) {
            if (separator == 0 || separator > 7 ||
                !std::ranges::all_of(
                    receive_buffer_.begin(),
                    receive_buffer_.begin() + separator,
                    [](unsigned char value) {
                        return value >= '0' && value <= '9';
                    }
                )) {
                throw std::runtime_error("invalid TCP frame prefix");
            }
            const std::size_t expected = std::stoull(
                receive_buffer_.substr(0, separator)
            ) + separator + 1;
            if (expected > 1024 * 1024 + 16) {
                throw std::runtime_error("oversized TCP frame");
            }
            if (receive_buffer_.size() >= expected) {
                const std::string frame = receive_buffer_.substr(0, expected);
                receive_buffer_.erase(0, expected);
                return {
                    TcpPollStatus::frame,
                    decode_lockstep_frame(frame),
                };
            }
        } else if (receive_buffer_.size() > 8) {
            throw std::runtime_error("invalid TCP frame prefix");
        }
        char buffer[4096];
        const auto received = ::recv(
            descriptor_, buffer, sizeof(buffer), 0
        );
        if (received < 0) {
            if (errno == EINTR) continue;
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                return {TcpPollStatus::no_data, std::nullopt};
            }
            throw socket_error("nonblocking receive");
        }
        if (received == 0) {
            close();
            if (!receive_buffer_.empty()) {
                throw std::runtime_error("truncated TCP frame");
            }
            return {TcpPollStatus::disconnected, std::nullopt};
        }
        receive_buffer_.append(buffer, static_cast<std::size_t>(received));
    }
}

TcpConnectStatus TcpFrameStream::connect_status() const {
    if (!open()) return TcpConnectStatus::failed;
    pollfd ready{descriptor_, POLLOUT, 0};
    const int result = ::poll(&ready, 1, 0);
    if (result == 0) return TcpConnectStatus::connecting;
    if (result < 0) {
        return errno == EINTR
            ? TcpConnectStatus::connecting
            : TcpConnectStatus::failed;
    }
    int error{};
    socklen_t error_size = sizeof(error);
    if (::getsockopt(
            descriptor_, SOL_SOCKET, SO_ERROR,
            &error, &error_size
        ) != 0 || error != 0) {
        return TcpConnectStatus::failed;
    }
    return TcpConnectStatus::connected;
}

void TcpFrameStream::send_frame_fragmented(
    const LockstepFrame& frame,
    std::size_t maximum_fragment
) {
    send_bytes(encode_lockstep_frame(frame), maximum_fragment);
}

std::optional<LockstepFrame> TcpFrameStream::receive_frame() {
    if (!open()) return std::nullopt;
    while (true) {
        std::optional<std::size_t> expected;
        {
            const auto separator = receive_buffer_.find(':');
            if (separator != std::string::npos) {
                if (separator == 0 || separator > 7 ||
                    !std::ranges::all_of(
                        receive_buffer_.begin(),
                        receive_buffer_.begin() + separator,
                        [](unsigned char value) {
                            return value >= '0' && value <= '9';
                        }
                    )) {
                    throw std::runtime_error("invalid TCP frame prefix");
                }
                expected = std::stoull(
                    receive_buffer_.substr(0, separator)
                ) +
                    separator + 1;
                if (*expected > 1024 * 1024 + 16) {
                    throw std::runtime_error("oversized TCP frame");
                }
            } else if (receive_buffer_.size() > 8) {
                throw std::runtime_error("invalid TCP frame prefix");
            }
        }
        if (expected && receive_buffer_.size() >= *expected) {
            const std::string frame = receive_buffer_.substr(0, *expected);
            receive_buffer_.erase(0, *expected);
            return decode_lockstep_frame(frame);
        }
        char buffer[257];
        const auto received = ::recv(
            descriptor_, buffer, sizeof(buffer), 0
        );
        if (received < 0) {
            if (errno == EINTR) continue;
            throw socket_error("receive");
        }
        if (received == 0) {
            close();
            if (receive_buffer_.empty()) return std::nullopt;
            throw std::runtime_error("truncated TCP frame");
        }
        receive_buffer_.append(buffer, static_cast<std::size_t>(received));
    }
}

TcpFrameListener::TcpFrameListener(std::uint16_t port) {
    descriptor_ = ::socket(AF_INET, SOCK_STREAM, 0);
    if (descriptor_ < 0) throw socket_error("socket");
    int reuse = 1;
    (void)::setsockopt(
        descriptor_, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)
    );
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = htons(port);
    if (::bind(
            descriptor_,
            reinterpret_cast<const sockaddr*>(&address),
            sizeof(address)
        ) != 0 ||
        ::listen(descriptor_, 8) != 0) {
        close_descriptor(descriptor_);
        throw socket_error("bind/listen");
    }
    socklen_t length = sizeof(address);
    if (::getsockname(
            descriptor_,
            reinterpret_cast<sockaddr*>(&address),
            &length
        ) != 0) {
        close_descriptor(descriptor_);
        throw socket_error("getsockname");
    }
    port_ = ntohs(address.sin_port);
}

TcpFrameListener::~TcpFrameListener() {
    close_descriptor(descriptor_);
}

TcpFrameListener::TcpFrameListener(TcpFrameListener&& other) noexcept
    : descriptor_(other.descriptor_), port_(other.port_),
      nonblocking_(other.nonblocking_) {
    other.descriptor_ = -1;
    other.port_ = 0;
}

TcpFrameListener& TcpFrameListener::operator=(
    TcpFrameListener&& other
) noexcept {
    if (this != &other) {
        close_descriptor(descriptor_);
        descriptor_ = other.descriptor_;
        port_ = other.port_;
        nonblocking_ = other.nonblocking_;
        other.descriptor_ = -1;
        other.port_ = 0;
    }
    return *this;
}

TcpFrameStream TcpFrameListener::accept() {
    const int peer = ::accept(descriptor_, nullptr, nullptr);
    if (peer < 0) throw socket_error("accept");
    try {
        configure_stream_descriptor(peer);
    } catch (...) {
        int owned = peer;
        close_descriptor(owned);
        throw;
    }
    return TcpFrameStream(peer);
}

void TcpFrameListener::set_nonblocking() {
    const int flags = ::fcntl(descriptor_, F_GETFL, 0);
    if (flags < 0 ||
        ::fcntl(descriptor_, F_SETFL, flags | O_NONBLOCK) != 0) {
        throw socket_error("fcntl listener nonblocking");
    }
    nonblocking_ = true;
}

std::optional<TcpFrameStream> TcpFrameListener::try_accept() {
    const int peer = ::accept(descriptor_, nullptr, nullptr);
    if (peer < 0) {
        if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) {
            return std::nullopt;
        }
        throw socket_error("accept");
    }
    try {
        configure_stream_descriptor(peer);
    } catch (...) {
        int owned = peer;
        close_descriptor(owned);
        throw;
    }
    TcpFrameStream stream(peer);
    stream.set_nonblocking();
    return stream;
}

TcpFrameStream connect_localhost(std::uint16_t port) {
    const int descriptor = ::socket(AF_INET, SOCK_STREAM, 0);
    if (descriptor < 0) throw socket_error("socket");
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = htons(port);
    if (::connect(
            descriptor,
            reinterpret_cast<const sockaddr*>(&address),
            sizeof(address)
        ) != 0) {
        int owned = descriptor;
        close_descriptor(owned);
        throw socket_error("connect");
    }
    try {
        configure_stream_descriptor(descriptor);
    } catch (...) {
        int owned = descriptor;
        close_descriptor(owned);
        throw;
    }
    return TcpFrameStream(descriptor);
}

std::optional<TcpFrameStream> try_connect_localhost(std::uint16_t port) {
    TcpFrameStream stream = begin_connect_localhost(port);
    if (stream.connect_status() != TcpConnectStatus::connected) {
        return std::nullopt;
    }
    return stream;
}

TcpFrameStream begin_connect_localhost(std::uint16_t port) {
    int descriptor = ::socket(AF_INET, SOCK_STREAM, 0);
    if (descriptor < 0) throw socket_error("socket");
    const int flags = ::fcntl(descriptor, F_GETFL, 0);
    if (flags < 0 ||
        ::fcntl(descriptor, F_SETFL, flags | O_NONBLOCK) != 0) {
        close_descriptor(descriptor);
        throw socket_error("fcntl connect nonblocking");
    }
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = htons(port);
    const int result = ::connect(
        descriptor,
        reinterpret_cast<const sockaddr*>(&address),
        sizeof(address)
    );
    if (result != 0 && errno != EINPROGRESS) {
        const int error = errno;
        close_descriptor(descriptor);
        throw std::runtime_error(
            "nonblocking connect failed with errno " +
            std::to_string(error)
        );
    }
    try {
        configure_stream_descriptor(descriptor);
    } catch (...) {
        close_descriptor(descriptor);
        throw;
    }
    TcpFrameStream stream(descriptor);
    stream.set_nonblocking();
    return stream;
}

#endif

LocalhostMultiPeerHost::LocalhostMultiPeerHost(
    std::uint16_t port,
    LockstepSessionConfig config,
    std::uint64_t timeout_steps,
    std::uint64_t hash_interval
) : listener_(port),
    config_(std::move(config)),
    host_slot_(config_.host_slot.value_or(
        *PlayerSlotId::from_index(0)
    )),
    session_(config_, timeout_steps, hash_interval) {
    if (!config_.native_roster ||
        !config_.native_roster->slot(host_slot_).occupied) {
        throw std::invalid_argument(
            "multi-peer host requires native occupied roster"
        );
    }
}

void LocalhostMultiPeerHost::accept_peer(PlayerSlotId slot) {
    const auto index = slot.index();
    if (!index || slot == host_slot_ ||
        !config_.native_roster->slot(slot).occupied ||
        streams_.contains(slot.stable_id())) {
        throw std::invalid_argument("invalid multi-peer slot");
    }
    TcpFrameStream stream = listener_.accept();
    stream.set_nonblocking();
    streams_.emplace(slot.stable_id(), std::move(stream));
}

void LocalhostMultiPeerHost::broadcast(
    const LockstepFrame& frame,
    std::optional<PlayerSlotId> except,
    std::size_t maximum_fragment
) {
    for (auto& [stable_id, stream] : streams_) {
        if (except && stable_id == except->stable_id()) continue;
        if (maximum_fragment != 0) {
            stream.send_frame_fragmented(frame, maximum_fragment);
        } else {
            stream.queue_frame(frame);
            (void)stream.flush_queued();
        }
    }
}

bool LocalhostMultiPeerHost::send(
    LockstepFrame frame,
    const Simulation& simulation,
    std::size_t maximum_fragment
) {
    frame.source = host_slot_;
    frame.player = player_slot_to_legacy(host_slot_)
        .value_or(Player::neutral);
    if (frame.kind == LockstepFrameKind::signal) {
        if (!frame.signal) return false;
        signal_log_.push_back(*frame.signal);
    } else if (!session_.receive(frame, simulation)) {
        return false;
    }
    broadcast(frame, std::nullopt, maximum_fragment);
    return true;
}

TcpFramePoll LocalhostMultiPeerHost::pump_one(
    const Simulation& simulation
) {
    for (auto& [stable_id, stream] : streams_) {
        TcpFramePoll result = stream.poll_frame();
        if (result.status == TcpPollStatus::no_data) continue;
        const PlayerSlotId bound =
            *decode_player_slot_id(stable_id);
        if (result.status == TcpPollStatus::disconnected) {
            session_.disconnect(bound);
            LockstepFrame disconnect;
            disconnect.kind = LockstepFrameKind::disconnect;
            disconnect.source = bound;
            disconnect.player = player_slot_to_legacy(bound)
                .value_or(Player::neutral);
            disconnect.scenario_digest = config_.scenario_digest;
            broadcast(disconnect, bound);
            return result;
        }
        if (!result.frame || !result.frame->source ||
            *result.frame->source != bound) {
            session_.disconnect(bound);
            throw std::runtime_error(
                "multi-peer frame source does not match bound slot"
            );
        }
        if (result.frame->kind == LockstepFrameKind::signal) {
            if (!result.frame->signal) {
                throw std::runtime_error(
                    "missing routed multi-peer signal"
                );
            }
            signal_log_.push_back(*result.frame->signal);
        } else if (!session_.receive(*result.frame, simulation)) {
            return {TcpPollStatus::no_data, std::nullopt};
        }
        broadcast(*result.frame, bound);
        return result;
    }
    return {TcpPollStatus::no_data, std::nullopt};
}

void LocalhostMultiPeerHost::close_peer(PlayerSlotId slot) {
    const auto found = streams_.find(slot.stable_id());
    if (found == streams_.end()) return;
    found->second.close();
    session_.disconnect(slot);
    LockstepFrame disconnect;
    disconnect.kind = LockstepFrameKind::disconnect;
    disconnect.source = slot;
    disconnect.player = player_slot_to_legacy(slot)
        .value_or(Player::neutral);
    disconnect.scenario_digest = config_.scenario_digest;
    broadcast(disconnect, slot);
}

LocalhostMultiPeerClient::LocalhostMultiPeerClient(
    std::uint16_t port,
    LockstepSessionConfig config,
    PlayerSlotId local_slot,
    std::uint64_t timeout_steps,
    std::uint64_t hash_interval
) : stream_(connect_localhost(port)),
    config_(std::move(config)),
    local_slot_(local_slot),
    session_(config_, timeout_steps, hash_interval) {
    if (!config_.native_roster || local_slot_.is_neutral() ||
        local_slot_ == config_.host_slot.value_or(
            *PlayerSlotId::from_index(0)
        ) ||
        !config_.native_roster->slot(local_slot_).occupied) {
        throw std::invalid_argument(
            "multi-peer client requires configured occupied slot"
        );
    }
    stream_.set_nonblocking();
}

bool LocalhostMultiPeerClient::send(
    LockstepFrame frame,
    const Simulation& simulation,
    std::size_t maximum_fragment
) {
    frame.source = local_slot_;
    frame.player = player_slot_to_legacy(local_slot_)
        .value_or(Player::neutral);
    if (frame.kind == LockstepFrameKind::signal) {
        if (!frame.signal) return false;
        signal_log_.push_back(*frame.signal);
    } else if (!session_.receive(frame, simulation)) {
        return false;
    }
    if (maximum_fragment != 0) {
        stream_.send_frame_fragmented(frame, maximum_fragment);
    } else {
        stream_.queue_frame(frame);
        (void)stream_.flush_queued();
    }
    return true;
}

TcpFramePoll LocalhostMultiPeerClient::pump_one(
    const Simulation& simulation
) {
    TcpFramePoll result = stream_.poll_frame();
    if (result.status == TcpPollStatus::disconnected) {
        const PlayerSlotId host = config_.host_slot.value_or(
            *PlayerSlotId::from_index(0)
        );
        session_.disconnect(host);
        return result;
    }
    if (result.status == TcpPollStatus::frame && result.frame) {
        if (!result.frame->source ||
            *result.frame->source == local_slot_) {
            throw std::runtime_error("invalid routed multi-peer frame");
        }
        if (result.frame->kind == LockstepFrameKind::signal) {
            if (!result.frame->signal) {
                throw std::runtime_error(
                    "missing routed multi-peer signal"
                );
            }
            signal_log_.push_back(*result.frame->signal);
        } else if (!session_.receive(*result.frame, simulation)) {
            return {TcpPollStatus::no_data, std::nullopt};
        }
    }
    return result;
}

LocalhostLockstepDriver::LocalhostLockstepDriver(
    TcpFrameStream stream,
    std::string scenario_digest,
    Player local_slot,
    bool host,
    std::uint64_t timeout_steps,
    std::uint64_t hash_interval
) : stream_(std::move(stream)),
    scenario_digest_(std::move(scenario_digest)),
    local_slot_(local_slot),
    remote_slot_(local_slot == Player::blue ? Player::red : Player::blue),
    host_(host),
    session_(scenario_digest_, timeout_steps, hash_interval) {
    if ((host_ && local_slot_ != Player::blue) ||
        (!host_ && local_slot_ != Player::red)) {
        throw std::invalid_argument(
            "localhost host must own blue and joiner must own red"
        );
    }
}

LocalhostLockstepDriver::LocalhostLockstepDriver(
    TcpFrameStream stream,
    LockstepSessionConfig config,
    Player local_slot,
    bool host,
    std::uint64_t timeout_steps,
    std::uint64_t hash_interval
) : stream_(std::move(stream)),
    scenario_digest_(config.scenario_digest),
    local_slot_(local_slot),
    remote_slot_(local_slot == Player::blue ? Player::red : Player::blue),
    host_(host),
    session_(std::move(config), timeout_steps, hash_interval) {
    if ((host_ && local_slot_ != Player::blue) ||
        (!host_ && local_slot_ != Player::red)) {
        throw std::invalid_argument(
            "localhost host must own blue and joiner must own red"
        );
    }
}

LockstepFrame LocalhostLockstepDriver::control_frame(
    LockstepFrameKind kind
) const {
    LockstepFrame frame;
    frame.kind = kind;
    frame.player = local_slot_;
    frame.scenario_digest = scenario_digest_;
    return frame;
}

bool LocalhostLockstepDriver::send_hello(const Simulation& simulation) {
    LockstepFrame frame = control_frame(LockstepFrameKind::hello);
    frame.config = session_.config();
    frame.config_digest = lockstep_config_digest(session_.config());
    if (!session_.receive(frame, simulation)) return false;
    stream_.send_frame(frame);
    return true;
}

bool LocalhostLockstepDriver::send_ready(const Simulation& simulation) {
    LockstepFrame frame = control_frame(LockstepFrameKind::ready);
    if (!session_.receive(frame, simulation)) return false;
    stream_.send_frame(frame);
    return true;
}

bool LocalhostLockstepDriver::send_start(const Simulation& simulation) {
    if (!host_) return false;
    LockstepFrame frame = control_frame(LockstepFrameKind::start);
    if (!session_.receive(frame, simulation)) return false;
    stream_.send_frame(frame);
    return true;
}

bool LocalhostLockstepDriver::submit_turn(
    const Simulation& simulation,
    std::vector<GameCommand> commands
) {
    return submit_turn_at(
        simulation, session_.current_tick(), std::move(commands)
    );
}

bool LocalhostLockstepDriver::submit_turn_at(
    const Simulation& simulation,
    std::uint64_t execution_tick,
    std::vector<GameCommand> commands
) {
    if (session_.status() != LockstepStatus::running) return false;
    LockstepFrame frame;
    frame.kind = LockstepFrameKind::turn;
    frame.player = local_slot_;
    frame.scenario_digest = scenario_digest_;
    frame.tick = execution_tick;
    frame.sequence = frame.tick;
    if (frame.tick == session_.current_tick() &&
        frame.tick % session_.hash_interval() == 0) {
        frame.state_hash = deterministic_state_hash(simulation);
    }
    frame.commands = std::move(commands);
    if (!session_.receive(frame, simulation)) return false;
    stream_.send_frame(frame);
    return true;
}

bool LocalhostLockstepDriver::send_chat(
    std::string text,
    ChatAudience audience
) {
    if (text.empty() || text.size() > 4096 || !valid_utf8(text) ||
        (session_.status() != LockstepStatus::ready &&
         session_.status() != LockstepStatus::running)) {
        return false;
    }
    LockstepFrame frame = control_frame(LockstepFrameKind::chat);
    frame.chat = LockstepChatMessage{
        host_ ? next_chat_sequence_++ : 0,
        local_slot_, audience, std::move(text),
    };
    if (host_) {
        last_chat_sequence_ = frame.chat->sequence;
        chat_log_.push_back(*frame.chat);
        const bool allied =
            session_.config().blue.team == session_.config().red.team;
        if (audience == ChatAudience::all || allied) {
            stream_.send_frame(frame);
        }
        return true;
    }
    stream_.send_frame(frame);
    return true;
}

bool LocalhostLockstepDriver::receive_chat(const LockstepFrame& frame) {
    if (!frame.chat || !frame.commands.empty() ||
        frame.tick != 0 || frame.sequence != 0 ||
        !frame.state_hash.empty() || frame.config ||
        !frame.config_digest.empty() ||
        frame.player != remote_slot_) {
        return false;
    }
    LockstepChatMessage message = *frame.chat;
    if (!valid_utf8(message.text)) return false;
    if (host_) {
        if (message.sequence != 0) return false;
        message.sequence = next_chat_sequence_++;
        LockstepFrame response = control_frame(LockstepFrameKind::chat);
        response.chat = message;
        stream_.send_frame(response);
        const bool allied =
            session_.config().blue.team == session_.config().red.team;
        if (message.audience == ChatAudience::all || allied) {
            last_chat_sequence_ = message.sequence;
            chat_log_.push_back(message);
        }
        return true;
    }
    if (message.sequence == 0 ||
        message.sequence <= last_chat_sequence_) {
        return false;
    }
    last_chat_sequence_ = message.sequence;
    chat_log_.push_back(std::move(message));
    return true;
}

bool LocalhostLockstepDriver::send_signal(
    TilePosition tile,
    ChatAudience audience
) {
    const bool allied =
        session_.config().blue.team == session_.config().red.team;
    if (tile.x < 0 || tile.y < 0 ||
        (audience == ChatAudience::allies && !allied) ||
        (session_.status() != LockstepStatus::ready &&
         session_.status() != LockstepStatus::running)) {
        return false;
    }
    const auto now = std::chrono::steady_clock::now();
    std::erase_if(signal_times_, [now](const auto sent) {
        return now - sent >= std::chrono::seconds(2);
    });
    if (signal_times_.size() >= 4) return false;
    signal_times_.push_back(now);
    LockstepFrame frame = control_frame(LockstepFrameKind::signal);
    frame.signal = LockstepMapSignal{
        host_ ? next_signal_sequence_++ : 0,
        local_slot_, audience, tile,
    };
    if (host_) {
        last_signal_sequence_ = frame.signal->sequence;
        signal_log_.push_back(*frame.signal);
        if (signal_log_.size() > 64) signal_log_.erase(signal_log_.begin());
        stream_.send_frame(frame);
        return true;
    }
    stream_.send_frame(frame);
    return true;
}

bool LocalhostLockstepDriver::receive_signal(
    const LockstepFrame& frame
) {
    if (!frame.signal || frame.chat || !frame.commands.empty() ||
        frame.tick != 0 || frame.sequence != 0 ||
        !frame.state_hash.empty() || frame.config ||
        !frame.config_digest.empty() ||
        frame.player != remote_slot_ ||
        frame.signal->tile.x < 0 || frame.signal->tile.y < 0) {
        return false;
    }
    LockstepMapSignal message = *frame.signal;
    if (host_) {
        if (message.sequence != 0 ||
            message.sender != remote_slot_) return false;
        message.sequence = next_signal_sequence_++;
        LockstepFrame response = control_frame(LockstepFrameKind::signal);
        response.signal = message;
        const bool allied =
            session_.config().blue.team == session_.config().red.team;
        if (message.audience == ChatAudience::all || allied) {
            stream_.send_frame(response);
            last_signal_sequence_ = message.sequence;
            signal_log_.push_back(message);
            if (signal_log_.size() > 64) {
                signal_log_.erase(signal_log_.begin());
            }
            return true;
        }
        return false;
    }
    if (message.sequence == 0 ||
        message.sequence <= last_signal_sequence_ ||
        (message.sender != Player::blue &&
         message.sender != Player::red)) {
        return false;
    }
    last_signal_sequence_ = message.sequence;
    signal_log_.push_back(std::move(message));
    if (signal_log_.size() > 64) signal_log_.erase(signal_log_.begin());
    return true;
}

bool LocalhostLockstepDriver::request_save_barrier(
    std::uint64_t target_tick
) {
    if (!host_ || session_.status() != LockstepStatus::running ||
        !save_barrier_.begin(target_tick, session_.current_tick())) {
        return false;
    }
    LockstepFrame frame = control_frame(
        LockstepFrameKind::save_barrier
    );
    frame.tick = target_tick;
    stream_.send_frame(frame);
    return true;
}

bool LocalhostLockstepDriver::submit_save_hash(
    const Simulation& simulation
) {
    if (save_hash_sent_ ||
        !save_barrier_.should_pause(session_.current_tick())) {
        return false;
    }
    SaveBarrierSubmission submission{
        save_barrier_.target_tick(),
        deterministic_state_hash(simulation),
        save_barrier_.target_tick() == 0
            ? 0 : save_barrier_.target_tick() - 1,
    };
    if (!save_barrier_.submit(local_slot_, submission)) return false;
    LockstepFrame frame = control_frame(LockstepFrameKind::save_hash);
    frame.tick = submission.tick;
    frame.sequence = submission.last_bundle_sequence;
    frame.state_hash = submission.state_hash;
    stream_.send_frame(frame);
    save_hash_sent_ = true;
    return true;
}

bool LocalhostLockstepDriver::receive_save_control(
    const LockstepFrame& frame
) {
    if (!frame.commands.empty() || frame.config ||
        !frame.config_digest.empty() || frame.chat || frame.signal) {
        return false;
    }
    if (frame.kind == LockstepFrameKind::save_barrier) {
        if (host_ || frame.player != Player::blue ||
            frame.sequence != 0 || !frame.state_hash.empty()) {
            return false;
        }
        return save_barrier_.begin(
            frame.tick, session_.current_tick()
        );
    }
    if (frame.kind != LockstepFrameKind::save_hash ||
        frame.player != remote_slot_ || frame.state_hash.empty()) {
        return false;
    }
    return save_barrier_.submit(frame.player, {
        frame.tick, frame.state_hash, frame.sequence,
    });
}

void LocalhostLockstepDriver::maintain_heartbeat(
    std::chrono::steady_clock::time_point now
) {
    if (session_.status() != LockstepStatus::ready &&
        session_.status() != LockstepStatus::running) {
        return;
    }
    if (last_heartbeat_sent_ !=
            std::chrono::steady_clock::time_point{} &&
        now - last_heartbeat_sent_ < std::chrono::seconds(1)) {
        return;
    }
    LockstepFrame frame = control_frame(
        LockstepFrameKind::heartbeat_ping
    );
    frame.sequence = next_heartbeat_sequence_++;
    pending_heartbeats_[frame.sequence] = now;
    while (pending_heartbeats_.size() > 8) {
        pending_heartbeats_.erase(pending_heartbeats_.begin());
    }
    stream_.send_frame(frame);
    last_heartbeat_sent_ = now;
}

bool LocalhostLockstepDriver::receive_heartbeat(
    const LockstepFrame& frame,
    std::chrono::steady_clock::time_point now
) {
    if (frame.player != remote_slot_ || frame.sequence == 0 ||
        frame.tick != 0 || !frame.state_hash.empty() ||
        !frame.commands.empty() || frame.config ||
        !frame.config_digest.empty() || frame.chat || frame.signal) {
        return false;
    }
    if (frame.kind == LockstepFrameKind::heartbeat_ping) {
        LockstepFrame response = control_frame(
            LockstepFrameKind::heartbeat_pong
        );
        response.sequence = frame.sequence;
        stream_.send_frame(response);
        return true;
    }
    const auto found = pending_heartbeats_.find(frame.sequence);
    if (frame.kind != LockstepFrameKind::heartbeat_pong ||
        found == pending_heartbeats_.end() || now < found->second) {
        return false;
    }
    round_trip_ms_ = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            now - found->second
        ).count()
    );
    pending_heartbeats_.erase(found);
    return true;
}

NetworkTimingMetrics LocalhostLockstepDriver::network_metrics(
    std::chrono::steady_clock::time_point now
) const {
    NetworkTimingMetrics metrics;
    metrics.round_trip_ms = round_trip_ms_;
    if (now >= last_peer_traffic_) {
        metrics.milliseconds_since_peer_traffic =
            static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    now - last_peer_traffic_
                ).count()
            );
    }
    metrics.waiting =
        metrics.milliseconds_since_peer_traffic >= 5000;
    if (round_trip_ms_) {
        metrics.latency_band =
            latency_band_for_rtt(*round_trip_ms_);
    }
    return metrics;
}

bool LocalhostLockstepDriver::propose_control(
    SessionControlMessage message
) {
    if (!host_ || pending_control_ ||
        session_.status() != LockstepStatus::running ||
        message.barrier_tick < session_.current_tick()) {
        return false;
    }
    const std::uint64_t delay = session_.config().input_delay_ticks;
    if (delay > 0 &&
        !(paused_ && message.kind == SessionControlKind::resume)) {
        const std::uint64_t current = session_.current_tick();
        if (current >=
            std::numeric_limits<std::uint64_t>::max() - delay) {
            return false;
        }
        message.barrier_tick = std::max(
            message.barrier_tick, current + delay + 1
        );
    }
    message.proposal_id = next_control_proposal_id_++;
    pending_control_ = message;
    control_acknowledged_ = false;
    control_committed_ = false;
    LockstepFrame frame = control_frame(
        LockstepFrameKind::control_proposal
    );
    frame.control = message;
    stream_.send_frame(frame);
    return true;
}

bool LocalhostLockstepDriver::propose_pause(
    bool paused,
    std::uint64_t barrier_tick
) {
    return propose_control({
        0, barrier_tick,
        paused ? SessionControlKind::pause
               : SessionControlKind::resume,
        game_speed_,
    });
}

bool LocalhostLockstepDriver::propose_speed(
    GameSpeed speed,
    std::uint64_t barrier_tick
) {
    return propose_control({
        0, barrier_tick, SessionControlKind::speed, speed,
    });
}

bool LocalhostLockstepDriver::receive_control(
    const LockstepFrame& frame
) {
    if (!frame.control || frame.player != remote_slot_ ||
        frame.tick != 0 || frame.sequence != 0 ||
        !frame.state_hash.empty() || !frame.commands.empty() ||
        frame.config || !frame.config_digest.empty() || frame.chat ||
        frame.signal) {
        return false;
    }
    const SessionControlMessage& message = *frame.control;
    if (frame.kind == LockstepFrameKind::control_proposal) {
        if (host_ || pending_control_ ||
            message.barrier_tick < session_.current_tick()) {
            return false;
        }
        pending_control_ = message;
        control_acknowledged_ = true;
        control_committed_ = false;
        LockstepFrame ack = control_frame(
            LockstepFrameKind::control_ack
        );
        ack.control = message;
        stream_.send_frame(ack);
        return true;
    }
    if (!pending_control_ ||
        message.proposal_id != pending_control_->proposal_id ||
        message.barrier_tick != pending_control_->barrier_tick ||
        message.kind != pending_control_->kind ||
        message.speed != pending_control_->speed) {
        return false;
    }
    if (frame.kind == LockstepFrameKind::control_ack) {
        if (!host_ || control_acknowledged_) return false;
        control_acknowledged_ = true;
        control_committed_ = true;
        LockstepFrame commit = control_frame(
            LockstepFrameKind::control_commit
        );
        commit.control = message;
        stream_.send_frame(commit);
        apply_committed_control();
        return true;
    }
    if (frame.kind != LockstepFrameKind::control_commit ||
        host_ || !control_acknowledged_ || control_committed_) {
        return false;
    }
    control_committed_ = true;
    apply_committed_control();
    return true;
}

void LocalhostLockstepDriver::apply_committed_control() {
    if (!pending_control_ || !control_committed_ ||
        session_.current_tick() < pending_control_->barrier_tick) {
        return;
    }
    if (pending_control_->kind == SessionControlKind::pause) {
        paused_ = true;
    } else if (pending_control_->kind ==
               SessionControlKind::resume) {
        paused_ = false;
    } else {
        game_speed_ = pending_control_->speed;
    }
    pending_control_.reset();
    control_acknowledged_ = false;
    control_committed_ = false;
}

void LocalhostLockstepDriver::update_reliability(
    std::chrono::steady_clock::time_point now
) {
    if (reliability_status_ ==
            MultiplayerReliabilityStatus::dropped ||
        reliability_status_ ==
            MultiplayerReliabilityStatus::disconnected) {
        return;
    }
    if (transport_lost_) {
        reliability_status_ =
            MultiplayerReliabilityStatus::suspended;
        reliability_reason_ =
            MultiplayerReliabilityReason::transport_lost;
        return;
    }
    const auto silent = now >= last_peer_traffic_
        ? now - last_peer_traffic_
        : std::chrono::steady_clock::duration::zero();
    if (silent >= std::chrono::seconds(30)) {
        reliability_status_ =
            MultiplayerReliabilityStatus::suspended;
        reliability_reason_ =
            MultiplayerReliabilityReason::peer_silent;
    } else if (silent >= std::chrono::seconds(5)) {
        reliability_status_ =
            MultiplayerReliabilityStatus::waiting;
        reliability_reason_ =
            MultiplayerReliabilityReason::peer_silent;
    } else {
        reliability_status_ =
            MultiplayerReliabilityStatus::active;
        reliability_reason_ =
            MultiplayerReliabilityReason::none;
    }
}

bool LocalhostLockstepDriver::drop_peer() {
    if (!host_ ||
        (reliability_status_ !=
             MultiplayerReliabilityStatus::waiting &&
         reliability_status_ !=
             MultiplayerReliabilityStatus::suspended)) {
        return false;
    }
    LockstepFrame frame = control_frame(LockstepFrameKind::peer_drop);
    try {
        stream_.send_frame(frame);
    } catch (const std::exception&) {
        // A suspended transport may already be gone. Local host decision is
        // still terminal; no replacement peer or AI is created.
    }
    reliability_status_ = MultiplayerReliabilityStatus::dropped;
    reliability_reason_ =
        MultiplayerReliabilityReason::host_dropped_peer;
    session_.disconnect(remote_slot_);
    return true;
}

bool LocalhostLockstepDriver::send_disconnect() {
    if (reliability_status_ ==
            MultiplayerReliabilityStatus::disconnected ||
        reliability_status_ ==
            MultiplayerReliabilityStatus::dropped) {
        return false;
    }
    LockstepFrame frame = control_frame(
        LockstepFrameKind::disconnect
    );
    try {
        stream_.send_frame(frame);
        (void)stream_.flush_queued();
    } catch (const std::exception&) {
    }
    reliability_status_ =
        MultiplayerReliabilityStatus::disconnected;
    reliability_reason_ =
        MultiplayerReliabilityReason::peer_disconnected;
    session_.disconnect(remote_slot_);
    return true;
}

bool LocalhostLockstepDriver::receive_drop(
    const LockstepFrame& frame
) {
    if (frame.player != remote_slot_ || frame.tick != 0 ||
        frame.sequence != 0 || !frame.state_hash.empty() ||
        !frame.commands.empty() || frame.config ||
        !frame.config_digest.empty() || frame.chat || frame.signal ||
        frame.control) {
        return false;
    }
    if (frame.kind == LockstepFrameKind::peer_drop) {
        if (host_ || frame.player != Player::blue) return false;
        reliability_status_ =
            MultiplayerReliabilityStatus::dropped;
        reliability_reason_ =
            MultiplayerReliabilityReason::host_dropped_peer;
    } else if (frame.kind == LockstepFrameKind::disconnect) {
        reliability_status_ =
            MultiplayerReliabilityStatus::disconnected;
        reliability_reason_ =
            MultiplayerReliabilityReason::peer_disconnected;
    } else {
        return false;
    }
    session_.disconnect(remote_slot_);
    return true;
}

bool LocalhostLockstepDriver::pump_one(const Simulation& simulation) {
    const auto frame = stream_.receive_frame();
    if (!frame) {
        session_.disconnect(remote_slot_);
        return false;
    }
    last_peer_traffic_ = std::chrono::steady_clock::now();
    if (frame->kind == LockstepFrameKind::chat) {
        return receive_chat(*frame);
    }
    if (frame->kind == LockstepFrameKind::signal) {
        return receive_signal(*frame);
    }
    if (frame->kind == LockstepFrameKind::save_barrier ||
        frame->kind == LockstepFrameKind::save_hash) {
        return receive_save_control(*frame);
    }
    if (frame->kind == LockstepFrameKind::heartbeat_ping ||
        frame->kind == LockstepFrameKind::heartbeat_pong) {
        return receive_heartbeat(
            *frame, std::chrono::steady_clock::now()
        );
    }
    if (frame->kind == LockstepFrameKind::control_proposal ||
        frame->kind == LockstepFrameKind::control_ack ||
        frame->kind == LockstepFrameKind::control_commit) {
        return receive_control(*frame);
    }
    if (frame->kind == LockstepFrameKind::peer_drop ||
        frame->kind == LockstepFrameKind::disconnect) {
        return receive_drop(*frame);
    }
    return session_.receive(*frame, simulation);
}

TcpFramePoll LocalhostLockstepDriver::pump_nonblocking(
    const Simulation& simulation
) {
    TcpFramePoll result = stream_.poll_frame();
    if (result.status == TcpPollStatus::frame) {
        last_peer_traffic_ = std::chrono::steady_clock::now();
    }
    if (result.status == TcpPollStatus::disconnected) {
        if (remote_eof_polls_++ >= disconnect_grace_polls_) {
            if (managed_drop_flow_) {
                transport_lost_ = true;
                reliability_status_ =
                    MultiplayerReliabilityStatus::suspended;
                reliability_reason_ =
                    MultiplayerReliabilityReason::transport_lost;
            } else {
                session_.disconnect(remote_slot_);
            }
        }
    } else if (result.status == TcpPollStatus::frame &&
               result.frame->kind == LockstepFrameKind::chat) {
        (void)receive_chat(*result.frame);
        remote_eof_polls_ = 0;
    } else if (result.status == TcpPollStatus::frame &&
               result.frame->kind == LockstepFrameKind::signal) {
        (void)receive_signal(*result.frame);
        remote_eof_polls_ = 0;
    } else if (result.status == TcpPollStatus::frame &&
               (result.frame->kind ==
                    LockstepFrameKind::peer_drop ||
                result.frame->kind ==
                    LockstepFrameKind::disconnect)) {
        (void)receive_drop(*result.frame);
        remote_eof_polls_ = 0;
    } else if (result.status == TcpPollStatus::frame &&
               (result.frame->kind ==
                    LockstepFrameKind::control_proposal ||
                result.frame->kind ==
                    LockstepFrameKind::control_ack ||
                result.frame->kind ==
                    LockstepFrameKind::control_commit)) {
        (void)receive_control(*result.frame);
        remote_eof_polls_ = 0;
    } else if (result.status == TcpPollStatus::frame &&
               (result.frame->kind ==
                    LockstepFrameKind::save_barrier ||
                result.frame->kind == LockstepFrameKind::save_hash)) {
        (void)receive_save_control(*result.frame);
        remote_eof_polls_ = 0;
    } else if (result.status == TcpPollStatus::frame &&
               (result.frame->kind ==
                    LockstepFrameKind::heartbeat_ping ||
                result.frame->kind ==
                    LockstepFrameKind::heartbeat_pong)) {
        (void)receive_heartbeat(
            *result.frame, std::chrono::steady_clock::now()
        );
        remote_eof_polls_ = 0;
    } else if (result.status == TcpPollStatus::frame &&
               !session_.receive(*result.frame, simulation)) {
        return result;
    } else if (result.status == TcpPollStatus::frame) {
        remote_eof_polls_ = 0;
    }
    return result;
}

bool LocalhostLockstepDriver::flush_outbound() {
    return stream_.flush_queued();
}

bool LocalhostLockstepDriver::advance(Simulation& simulation) {
    if (reliability_status_ !=
            MultiplayerReliabilityStatus::active ||
        paused_ || control_barrier_waiting()) {
        return false;
    }
    return session_.advance(simulation);
}

void LocalhostLockstepDriver::close() {
    stream_.close();
    session_.disconnect(remote_slot_);
}

LocalhostMultiplayerRuntime::LocalhostMultiplayerRuntime(
    bool host,
    std::uint16_t port,
    std::string scenario_digest,
    std::uint64_t timeout_steps,
    std::uint64_t hash_interval,
    bool automatic_flow
) : host_(host),
    port_(port),
    scenario_digest_(std::move(scenario_digest)),
    timeout_steps_(timeout_steps),
    hash_interval_(hash_interval),
    automatic_flow_(automatic_flow) {
    config_.scenario_digest = scenario_digest_;
    if (host_) {
        listener_.emplace(port_);
        listener_->set_nonblocking();
        port_ = listener_->port();
    }
}

LocalhostMultiplayerRuntime::LocalhostMultiplayerRuntime(
    bool host,
    std::uint16_t port,
    LockstepSessionConfig config,
    std::uint64_t timeout_steps,
    std::uint64_t hash_interval,
    bool automatic_flow
) : host_(host),
    port_(port),
    scenario_digest_(config.scenario_digest),
    config_(std::move(config)),
    timeout_steps_(timeout_steps),
    hash_interval_(hash_interval),
    automatic_flow_(automatic_flow) {
    (void)canonical_lockstep_config(config_);
    if (host_) {
        listener_.emplace(port_);
        listener_->set_nonblocking();
        port_ = listener_->port();
    }
}

LocalhostMultiplayerRuntime LocalhostMultiplayerRuntime::host(
    std::uint16_t port,
    std::string scenario_digest,
    std::uint64_t timeout_steps,
    std::uint64_t hash_interval
) {
    return {
        true, port, std::move(scenario_digest),
        timeout_steps, hash_interval, true,
    };
}

LocalhostMultiplayerRuntime LocalhostMultiplayerRuntime::join(
    std::uint16_t port,
    std::string scenario_digest,
    std::uint64_t timeout_steps,
    std::uint64_t hash_interval
) {
    return {
        false, port, std::move(scenario_digest),
        timeout_steps, hash_interval, true,
    };
}

LocalhostMultiplayerRuntime LocalhostMultiplayerRuntime::host(
    std::uint16_t port,
    LockstepSessionConfig config,
    std::uint64_t timeout_steps,
    std::uint64_t hash_interval
) {
    return {
        true, port, std::move(config),
        timeout_steps, hash_interval, false,
    };
}

LocalhostMultiplayerRuntime LocalhostMultiplayerRuntime::join(
    std::uint16_t port,
    LockstepSessionConfig config,
    std::uint64_t timeout_steps,
    std::uint64_t hash_interval
) {
    return {
        false, port, std::move(config),
        timeout_steps, hash_interval, false,
    };
}

void LocalhostMultiplayerRuntime::attach(TcpFrameStream stream) {
    stream.set_nonblocking();
    driver_.emplace(
        std::move(stream),
        config_,
        host_ ? Player::blue : Player::red,
        host_,
        timeout_steps_,
        hash_interval_
    );
    // Frame-loop clients may close immediately after writing a terminal
    // proof. Keep the already-committed state observable briefly while direct
    // drivers retain immediate disconnect reporting.
    driver_->set_disconnect_grace_polls(64);
    driver_->set_managed_drop_flow(true);
}

bool LocalhostMultiplayerRuntime::queue_command(GameCommand command) {
    if (local_controller_state_ != PlayerControllerState::active ||
        local_resignation_pending_) {
        return false;
    }
    if (pending_commands_.size() >= 256) {
        throw std::runtime_error("local multiplayer command queue full");
    }
    const Player local = host_ ? Player::blue : Player::red;
    const bool resigning = std::holds_alternative<ResignCommand>(command) &&
        std::get<ResignCommand>(command).player == local;
    pending_commands_.push_back(std::move(command));
    local_resignation_pending_ = resigning;
    return true;
}

void LocalhostMultiplayerRuntime::poll_transport(Simulation& simulation) {
    pump_impl(simulation, false);
}

void LocalhostMultiplayerRuntime::pump(Simulation& simulation) {
    pump_impl(simulation, true);
}

void LocalhostMultiplayerRuntime::pump_impl(
    Simulation& simulation,
    bool allow_turn
) {
    const Player local = host_ ? Player::blue : Player::red;
    if (simulation.controller_state(local) !=
        PlayerControllerState::active) {
        local_controller_state_ = PlayerControllerState::observer;
        local_resignation_pending_ = false;
        pending_commands_.clear();
    }
    if (!driver_) {
        if (host_) {
            if (auto stream = listener_->try_accept()) {
                attach(std::move(*stream));
            } else {
                return;
            }
        } else {
            try {
                if (!pending_connection_) {
                    pending_connection_.emplace(
                        begin_connect_localhost(port_)
                    );
                }
                const TcpConnectStatus connection =
                    pending_connection_->connect_status();
                if (connection == TcpConnectStatus::connecting) return;
                if (connection == TcpConnectStatus::failed) {
                    pending_connection_.reset();
                    return;
                }
                attach(std::move(*pending_connection_));
                pending_connection_.reset();
            } catch (const std::exception&) {
                pending_connection_.reset();
                return;
            }
        }
    }
    try {
        driver_->maintain_heartbeat();
        driver_->update_reliability();
        (void)driver_->flush_outbound();
        if (simulation.controller_state(local) !=
            PlayerControllerState::active) {
            local_controller_state_ = PlayerControllerState::observer;
            local_resignation_pending_ = false;
            pending_commands_.clear();
        }
        if (!hello_sent_) {
            hello_sent_ = driver_->send_hello(simulation);
        }
        // Commit between inbound frames. In particular, do not consume a
        // peer's orderly EOF after its final turn before applying that turn.
        // The frame loop calls this runtime repeatedly, so one frame per pump
        // remains nonblocking while preserving the final-turn barrier.
        const TcpFramePoll inbound =
            driver_->pump_nonblocking(simulation);
        const Player remote = host_ ? Player::red : Player::blue;
        if (!ready_sent_ && (automatic_flow_ || ready_requested_) &&
            driver_->session().connected(remote)) {
            ready_sent_ = driver_->send_ready(simulation);
        }
        if (host_ && (automatic_flow_ || start_requested_) && ready_sent_ &&
            driver_->status() == LockstepStatus::ready &&
            !start_sent_) {
            start_sent_ = driver_->send_start(simulation);
        }
        driver_->update_control_barrier();
        if (driver_->save_barrier().should_pause(
                driver_->session().current_tick()
            )) {
            (void)driver_->submit_save_hash(simulation);
            (void)driver_->flush_outbound();
            return;
        }
        if (driver_->reliability_status() ==
                MultiplayerReliabilityStatus::waiting ||
            driver_->reliability_status() ==
                MultiplayerReliabilityStatus::suspended) {
            (void)driver_->flush_outbound();
            return;
        }
        if (driver_->paused() ||
            driver_->control_barrier_waiting()) {
            (void)driver_->flush_outbound();
            return;
        }
        if (allow_turn &&
            driver_->status() == LockstepStatus::running) {
            const std::uint64_t tick = driver_->session().current_tick();
            const std::uint64_t scheduled =
                tick + static_cast<std::uint64_t>(
                    driver_->session().config().input_delay_ticks
                );
            while (next_submission_tick_ <= scheduled) {
                std::vector<GameCommand> commands;
                if (next_submission_tick_ == scheduled) {
                    commands = std::move(pending_commands_);
                }
                if (driver_->submit_turn_at(
                        simulation, next_submission_tick_,
                        std::move(commands)
                    )) {
                    if (next_submission_tick_ == scheduled) {
                        pending_commands_.clear();
                    }
                    submitted_ticks_.insert(next_submission_tick_);
                    ++next_submission_tick_;
                } else {
                    break;
                }
            }
            // As above, never let an EOF overtake the turn immediately
            // before it within one pump/commit cycle.
            if (inbound.status != TcpPollStatus::frame) {
                (void)driver_->pump_nonblocking(simulation);
            }
            if (driver_->advance(simulation)) {
                submitted_ticks_.erase(tick);
            }
        }
        (void)driver_->flush_outbound();
    } catch (const std::exception&) {
        driver_->close();
    }
}

void LocalhostMultiplayerRuntime::disconnect() {
    if (driver_) {
        (void)driver_->send_disconnect();
        driver_->close();
    }
}

bool LocalhostMultiplayerRuntime::send_chat(
    std::string text,
    ChatAudience audience
) {
    return local_controller_state_ == PlayerControllerState::active &&
        !local_resignation_pending_ &&
        driver_ &&
        driver_->send_chat(std::move(text), audience);
}

bool LocalhostMultiplayerRuntime::send_signal(
    TilePosition tile,
    ChatAudience audience
) {
    return local_controller_state_ == PlayerControllerState::active &&
        !local_resignation_pending_ &&
        driver_ &&
        driver_->send_signal(tile, audience);
}

bool LocalhostMultiplayerRuntime::request_save_barrier(
    std::uint64_t target_tick
) {
    return driver_ &&
        driver_->request_save_barrier(target_tick);
}

bool LocalhostMultiplayerRuntime::propose_pause(
    bool paused,
    std::uint64_t barrier_tick
) {
    return driver_ &&
        driver_->propose_pause(paused, barrier_tick);
}

bool LocalhostMultiplayerRuntime::propose_speed(
    GameSpeed speed,
    std::uint64_t barrier_tick
) {
    return driver_ &&
        driver_->propose_speed(speed, barrier_tick);
}

bool LocalhostMultiplayerRuntime::drop_peer() {
    return driver_ && driver_->drop_peer();
}

const std::vector<LockstepChatMessage>&
LocalhostMultiplayerRuntime::chat_log() const {
    static const std::vector<LockstepChatMessage> empty;
    return driver_ ? driver_->chat_log() : empty;
}

const std::vector<LockstepMapSignal>&
LocalhostMultiplayerRuntime::signal_log() const {
    static const std::vector<LockstepMapSignal> empty;
    return driver_ ? driver_->signal_log() : empty;
}

LockstepStatus LocalhostMultiplayerRuntime::status() const {
    return driver_ ? driver_->status() : LockstepStatus::handshaking;
}

std::uint64_t LocalhostMultiplayerRuntime::current_tick() const {
    return driver_ ? driver_->session().current_tick() : 0;
}

bool LocalhostMultiplayerRuntime::waiting_for_turn() const {
    return driver_ &&
        driver_->status() == LockstepStatus::running &&
        submitted_ticks_.contains(
            driver_->session().current_tick()
        );
}

}  // namespace aoe
