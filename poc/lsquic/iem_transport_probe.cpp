// Isolated Windows QUIC/TLS proof for the IEM audio payload.
// The embedded pairing token and generated certificate are test-only; this is not a production credential flow.
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>
#include <winsock2.h>
#include <sys/queue.h>
#include "lsquic.h"
#include "../src/liblsquic/lsquic_hash.h"
#ifdef __cplusplus
extern "C" {
#endif
#include "test_common.h"
#include "test_cert.h"
#include "prog.h"
#ifdef __cplusplus
}
#endif
#include <openssl/pem.h>
#include <openssl/x509.h>
#include <openssl/ssl.h>
#include "AudioPacketCodec.h"
#include "TransportSecurityPolicy.h"

struct App;
struct lsquic_conn_ctx {
    App* app = nullptr;
    DbStudioSecurity::TransportSecurityGate gate{};
    bool authorized = false;
    bool peerVerified = false;
    lsquic_stream_t* controlStream = nullptr;
    std::array<std::uint8_t, DbStudioNet::kMaxDatagramBytes> pending{};
    std::size_t pendingSize = 0;
};
struct lsquic_stream_ctx {
    lsquic_stream_t* stream = nullptr;
    bool server = false;
    std::string input;
    std::string output;
    std::size_t outputOffset = 0;
};
struct App {
    struct prog program{};
    struct sport_head sports{};
    bool server = false;
    bool badPin = false;
    bool badToken = false;
    int exitCode = 1;
    X509* pinnedCertificate = nullptr;
    std::string certificatePath;
    std::string keyPath;
};

namespace {
constexpr char kAlpn[] = "dbs-iem-poc/1";
constexpr char kPairingProof[] = "PAIR:demo-device-01:aux=0\n";
constexpr char kAuthReply[] = "AUTH_OK\n";
constexpr char kServerName[] = "iem-poc.local";
constexpr std::size_t kMaxControl = 128;

App* g_app = nullptr;

int verifyPinnedPeer(int preverifyOk, X509_STORE_CTX* store) {
    (void) preverifyOk;
    if (!g_app || !g_app->pinnedCertificate || g_app->badPin)
        return 0;
    if (X509_STORE_CTX_get_error_depth(store) != 0)
        return 0;
    X509* peer = X509_STORE_CTX_get_current_cert(store);
    if (!peer || X509_cmp(peer, g_app->pinnedCertificate) != 0)
        return 0;
    if (X509_check_host(peer, kServerName, sizeof(kServerName) - 1, 0, nullptr) != 1)
        return 0;
    X509_STORE_CTX_set_error(store, X509_V_OK);
    return 1;
}

void finishClient(lsquic_conn_t* conn, int result) {
    auto* context = static_cast<lsquic_conn_ctx_t*>(lsquic_conn_get_ctx(conn));
    if (context && context->app) {
        context->app->exitCode = result;
        if (result == 0)
            std::cout << "QUIC_TLS_DEVICE_AUTH_AND_AUDIO_DATAGRAM_OK\n";
    }
    lsquic_conn_close(conn);
}

bool makeAudioPacket(std::array<std::uint8_t, DbStudioNet::kMaxDatagramBytes>& bytes,
                     std::size_t& size) {
    using namespace DbStudioNet;
    PacketMetadata metadata{};
    for (std::size_t i = 0; i < metadata.sessionId.size(); ++i)
        metadata.sessionId[i] = static_cast<std::uint8_t>(i + 1);
    metadata.streamId = 0;
    metadata.sequence = 7;
    metadata.timestampFrames = 4096;
    metadata.sampleRate = 48000;
    metadata.frames = 16;
    std::array<float, 16> left{}, right{};
    for (std::size_t i = 0; i < left.size(); ++i) {
        left[i] = 0.01f * static_cast<float>(i + 1);
        right[i] = -left[i];
    }
    return encodeAudioPacket(metadata, left.data(), right.data(), bytes.data(),
                             bytes.size(), size) == PacketResult::success;
}

bool validAudioPacket(const std::uint8_t* data, std::size_t size) {
    using namespace DbStudioNet;
    PacketMetadata metadata{};
    std::array<float, kMaxFramesPerPacket> left{}, right{};
    if (decodeAudioPacket(data, size, metadata, left.data(), right.data(), left.size())
            != PacketResult::success)
        return false;
    if (metadata.streamId != 0 || metadata.sequence != 7 ||
        metadata.timestampFrames != 4096 || metadata.sampleRate != 48000 ||
        metadata.frames != 16)
        return false;
    for (std::size_t i = 0; i < metadata.frames; ++i)
        if (left[i] != 0.01f * static_cast<float>(i + 1) ||
            right[i] != -left[i])
            return false;
    return true;
}

lsquic_conn_ctx_t* onNewConnection(void* opaque, lsquic_conn_t* conn) {
    auto* app = static_cast<App*>(opaque);
    auto* context = new lsquic_conn_ctx{};
    context->app = app;
    lsquic_conn_set_ctx(conn, context);
    if (app->server) {
        // Server TLS is complete here. Device identity still requires the authenticated control stream.
        std::cout << "TLS 1.3 QUIC peer connected; awaiting device pairing proof\n";
    } else {
        std::cout << "Client QUIC connection created\n";
        context->gate.beginHandshake();
        // LSQUIC creates client streams from this callback reliably. The stream
        // remains write-disabled until onHandshakeDone verifies the pinned peer.
        lsquic_conn_make_stream(conn);
        std::cerr << "Client requested control stream; pending="
                  << lsquic_conn_n_pending_streams(conn) << "\n";
    }
    return context;
}

void onConnectionClosed(lsquic_conn_t* conn) {
    auto* context = static_cast<lsquic_conn_ctx_t*>(lsquic_conn_get_ctx(conn));
    if (!context)
        return;
    App* app = context->app;
    lsquic_conn_set_ctx(conn, nullptr);
    delete context;
    if (app) {
        if (app->server)
            std::cout << "QUIC device session closed\n";
        prog_stop(&app->program);
    }
}

lsquic_stream_ctx_t* onNewStream(void* opaque, lsquic_stream_t* stream) {
    auto* app = static_cast<App*>(opaque);
    auto* context = new lsquic_stream_ctx{};
    context->stream = stream;
    context->server = app->server;
    lsquic_stream_set_ctx(stream, context);
    std::cerr << (app->server ? "Server" : "Client")
              << " control stream callback\n";
    if (app->server) {
        lsquic_stream_wantread(stream, 1);
    } else {
        auto* connection = static_cast<lsquic_conn_ctx_t*>(
            lsquic_conn_get_ctx(lsquic_stream_conn(stream)));
        if (!connection) {
            delete context;
            lsquic_stream_close(stream);
            return nullptr;
        }
        connection->controlStream = stream;
        context->output = app->badToken ? "PAIR:unpaired-device:aux=0\n" : kPairingProof;
        if (connection->peerVerified)
            lsquic_stream_wantwrite(stream, 1);
    }
    return context;
}

void onStreamRead(lsquic_stream_t* stream, lsquic_stream_ctx_t* context) {
    auto* connection = static_cast<lsquic_conn_ctx_t*>(
        lsquic_conn_get_ctx(lsquic_stream_conn(stream)));
    if (!connection || !context)
        return;
    std::array<char, 64> chunk{};
    const auto count = lsquic_stream_read(stream, chunk.data(), chunk.size());
    std::cerr << (context->server ? "Server" : "Client")
              << " stream read callback bytes=" << count << "\n";
    if (count == 0)
        return;
    if (count < 0 || context->input.size() + static_cast<std::size_t>(count) > kMaxControl) {
        if (!context->server)
            finishClient(lsquic_stream_conn(stream), 1);
        else
            lsquic_conn_close(lsquic_stream_conn(stream));
        return;
    }
    context->input.append(chunk.data(), static_cast<std::size_t>(count));

    if (context->server) {
        if (context->input.size() < sizeof(kPairingProof) - 1)
            return;
        if (context->input != kPairingProof) {
            std::cerr << "Device authentication rejected\n";
            lsquic_conn_close(lsquic_stream_conn(stream));
            return;
        }
        connection->authorized = true;
        context->output = kAuthReply;
        lsquic_stream_wantread(stream, 0);
        lsquic_stream_wantwrite(stream, 1);
        std::cout << "Device pairing proof accepted; AUX 0 authorized\n";
    } else {
        if (context->input.size() < sizeof(kAuthReply) - 1)
            return;
        if (context->input != kAuthReply ||
            !connection->gate.markDeviceAuthenticated(true) ||
            !connection->gate.markAuthorized(true)) {
            finishClient(lsquic_stream_conn(stream), 1);
            return;
        }
        // The shared DATAGRAM writer enforces this flag on both roles. Set it
        // only after the server has accepted the device proof and authorized AUX 0.
        connection->authorized = true;
        std::array<std::uint8_t, DbStudioNet::kMaxDatagramBytes> packet{};
        std::size_t packetSize = 0;
        if (!connection->gate.maySend(DbStudioSecurity::DataClass::audioDatagram, false) ||
            !makeAudioPacket(packet, packetSize)) {
            finishClient(lsquic_stream_conn(stream), 1);
            return;
        }
        connection->pending = packet;
        connection->pendingSize = packetSize;
        lsquic_conn_want_datagram_write(lsquic_stream_conn(stream), 1);
    }
}

void onStreamWrite(lsquic_stream_t* stream, lsquic_stream_ctx_t* context) {
    if (!context)
        return;
    auto* connection = static_cast<lsquic_conn_ctx_t*>(
        lsquic_conn_get_ctx(lsquic_stream_conn(stream)));
    if (!connection)
        return;
    if (context->outputOffset < context->output.size()) {
        std::cerr << (context->server ? "Server" : "Client")
                  << " stream write callback remaining="
                  << (context->output.size() - context->outputOffset) << "\n";
        const auto remaining = context->output.size() - context->outputOffset;
        const auto written = lsquic_stream_write(stream,
            context->output.data() + context->outputOffset, remaining);
        if (written < 0) {
            lsquic_conn_close(lsquic_stream_conn(stream));
            return;
        }
        context->outputOffset += static_cast<std::size_t>(written);
        if (context->outputOffset < context->output.size())
            return;
        lsquic_stream_flush(stream);
        lsquic_stream_wantwrite(stream, 0);
        if (!context->server)
            lsquic_stream_wantread(stream, 1);
    }
}

void onStreamClose(lsquic_stream_t*, lsquic_stream_ctx_t* context) {
    delete context;
}

ssize_t onDatagramWrite(lsquic_conn_t* conn, void* buffer, size_t capacity) {
    auto* context = static_cast<lsquic_conn_ctx_t*>(lsquic_conn_get_ctx(conn));
    if (!context || !context->authorized || context->pendingSize == 0 ||
        context->pendingSize > capacity)
        return -1;
    std::memcpy(buffer, context->pending.data(), context->pendingSize);
    const auto size = static_cast<ssize_t>(context->pendingSize);
    context->pendingSize = 0;
    lsquic_conn_want_datagram_write(conn, 0);
    return size;
}

void onDatagram(lsquic_conn_t* conn, const void* buffer, size_t size) {
    auto* context = static_cast<lsquic_conn_ctx_t*>(lsquic_conn_get_ctx(conn));
    if (!context || !context->app)
        return;
    const auto* bytes = static_cast<const std::uint8_t*>(buffer);
    if (context->app->server) {
        if (!context->authorized || !validAudioPacket(bytes, size) ||
            size > context->pending.size()) {
            std::cerr << "Rejected invalid or unauthorized audio datagram\n";
            lsquic_conn_close(conn);
            return;
        }
        std::memcpy(context->pending.data(), bytes, size);
        context->pendingSize = size;
        lsquic_conn_want_datagram_write(conn, 1);
    } else {
        if (!validAudioPacket(bytes, size)) {
            finishClient(conn, 1);
            return;
        }
        finishClient(conn, 0);
    }
}

void onHandshakeDone(lsquic_conn_t* conn, enum lsquic_hsk_status status) {
    auto* context = static_cast<lsquic_conn_ctx_t*>(lsquic_conn_get_ctx(conn));
    if (!context || !context->app || context->app->server)
        return;
    std::cout << "Client handshake status: " << static_cast<int>(status) << "\n";
    if ((status != LSQ_HSK_OK && status != LSQ_HSK_RESUMED_OK) ||
        !context->gate.markPeerVerified(true)) {
        std::cerr << "TLS peer verification failed\n";
        finishClient(conn, 1);
        return;
    }
    context->peerVerified = true;
    const auto pending = lsquic_conn_n_pending_streams(conn);
    std::cerr << "Client peer verified; controlStream="
              << (context->controlStream != nullptr) << ", pending=" << pending << "\n";
    if (context->controlStream) {
        lsquic_stream_wantwrite(context->controlStream, 1);
    } else if (pending == 0) {
        std::cerr << "Client requesting fallback control stream after handshake\n";
        lsquic_conn_make_stream(conn);
    }
}

const lsquic_stream_if streamInterface = {
    onNewConnection, nullptr, onConnectionClosed, onNewStream,
    onStreamRead, onStreamWrite, onStreamClose, onDatagramWrite,
    onDatagram, onHandshakeDone
};

int run(bool server, const char* address, const char* cert, const char* key,
        const char* trust, bool badPin, bool badToken) {
    App app{};
    app.server = server;
    app.badPin = badPin;
    app.badToken = badToken;
    app.certificatePath = cert ? cert : "";
    app.keyPath = key ? key : "";
    g_app = &app;
    TAILQ_INIT(&app.sports);

    if (prog_init(&app.program, server ? LSENG_SERVER : 0, &app.sports,
                  &streamInterface, &app) != 0) {
        std::cerr << "probe initialization failed: prog_init\n" << std::flush;
        return 2;
    }
    app.program.prog_settings.es_datagrams = 1;
    app.program.prog_api.ea_alpn = kAlpn;

    if (server) {
        _putenv_s("LSQUIC_ENABLE_EARLY_DATA", "0");
        std::string spec = std::string(kServerName) + "," + cert + "," + key;
        if (prog_set_opt(&app.program, 's', address) != 0) {
            std::cerr << "probe initialization failed: server service port\n" << std::flush;
            return 2;
        }
        if (prog_set_opt(&app.program, 'c', spec.c_str()) != 0) {
            std::cerr << "probe initialization failed: server certificate/key\n" << std::flush;
            return 2;
        }
        if (add_alpn(kAlpn) != 0) {
            std::cerr << "probe initialization failed: server ALPN\n" << std::flush;
            return 2;
        }
        if (prog_prep(&app.program) != 0) {
            std::cerr << "probe initialization failed: server prog_prep\n" << std::flush;
            return 2;
        }
        auto* ssl = static_cast<SSL_CTX*>(lookup_cert(
            app.program.prog_certs, nullptr, kServerName));
        if (!ssl) {
            std::cerr << "probe initialization failed: server certificate lookup\n" << std::flush;
            return 2;
        }
        SSL_CTX_set_early_data_enabled(ssl, 0);
    } else {
        if (prog_set_opt(&app.program, 's', address) != 0) {
            std::cerr << "probe initialization failed: client service port\n" << std::flush;
            return 2;
        }
        if (prog_set_opt(&app.program, 'H', kServerName) != 0) {
            std::cerr << "probe initialization failed: client hostname\n" << std::flush;
            return 2;
        }
        if (prog_prep(&app.program) != 0) {
            std::cerr << "probe initialization failed: client prog_prep\n" << std::flush;
            return 2;
        }
        FILE* file = std::fopen(trust, "rb");
        if (!file) {
            std::cerr << "probe initialization failed: opening pinned certificate\n" << std::flush;
            return 2;
        }
        app.pinnedCertificate = PEM_read_X509(file, nullptr, nullptr, nullptr);
        std::fclose(file);
        if (!app.pinnedCertificate) {
            std::cerr << "probe initialization failed: parsing pinned certificate\n" << std::flush;
            return 2;
        }
        SSL_CTX_set_verify(app.program.prog_ssl_ctx, SSL_VERIFY_PEER,
                           verifyPinnedPeer);
        SSL_CTX_set_early_data_enabled(app.program.prog_ssl_ctx, 0);
    }

    if (server) {
        const int runResult = prog_run(&app.program);
        std::cerr << "probe server event loop returned: " << runResult << "\n" << std::flush;
        if (runResult != 0)
            app.exitCode = 1;
    } else {
        const int connectResult = prog_connect(&app.program, nullptr, 0);
        std::cerr << "probe client connect setup result: " << connectResult << "\n" << std::flush;
        if (connectResult != 0)
            return 2;
        const int runResult = prog_run(&app.program);
        std::cerr << "probe client event loop returned: " << runResult
                  << ", final status=" << app.exitCode << "\n" << std::flush;
    }
    prog_cleanup(&app.program);
    if (app.pinnedCertificate)
        X509_free(app.pinnedCertificate);
    g_app = nullptr;
    return app.exitCode;
}
} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "probe invalid arguments: argc=" << argc << "\n" << std::flush;
        return 2;
    }
    const std::string mode = argv[1];
    std::cerr << "probe mode=" << mode << " argc=" << argc << "\n" << std::flush;
    if (mode == "server" && argc == 5)
        return run(true, argv[2], argv[3], argv[4], nullptr, false, false);
    if (mode == "client" && argc >= 4) {
        bool badPin = false, badToken = false;
        for (int i = 4; i < argc; ++i) {
            badPin = badPin || std::string(argv[i]) == "--bad-pin";
            badToken = badToken || std::string(argv[i]) == "--bad-token";
        }
        return run(false, argv[2], nullptr, nullptr, argv[3], badPin, badToken);
    }
    std::cerr << "probe unsupported argument combination: mode=" << mode
              << " argc=" << argc << "\n" << std::flush;
    return 2;
}
