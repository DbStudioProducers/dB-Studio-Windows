package com.dbstudio.receiverbench

import android.content.Context
import android.net.nsd.NsdManager
import android.net.nsd.NsdServiceInfo
import android.os.Handler
import android.os.Looper
import java.net.Inet4Address
import java.net.InetAddress
import java.util.concurrent.atomic.AtomicBoolean

/**
 * LAN discovery only. Finding an mDNS record is NOT authentication.
 * Callers must establish the authenticated QUIC session and validate the
 * server identity before showing a connected state or accepting audio.
 */
data class DiscoveredDbStudioServer(
    val serviceName: String,
    val hostAddress: String,
    val port: Int,
    val protocolVersion: String,
    val installationId: String,
    val state: String,
    val capacity: Int
)

class DbStudioDiscovery(context: Context, private val listener: Listener) {
    interface Listener {
        fun onServerFound(server: DiscoveredDbStudioServer)
        fun onServerLost(serviceName: String)
        fun onDiscoveryError(message: String)
    }

    companion object {
        const val SERVICE_TYPE = "_dbstudio._udp."
        private const val REQUIRED_PROTOCOL = "1"
    }

    private val manager = context.applicationContext.getSystemService(Context.NSD_SERVICE) as NsdManager
    private val stopped = AtomicBoolean(true)
    // Prevent duplicate discoverServices calls while Android is still starting discovery.
    private val requested = AtomicBoolean(false)
    private val mainHandler = Handler(Looper.getMainLooper())

    private fun notifyError(message: String) {
        mainHandler.post { if (!stopped.get()) listener.onDiscoveryError(message) }
    }
    private val resolving = HashSet<String>()
    private val known = HashSet<String>()

    private val discoveryListener = object : NsdManager.DiscoveryListener {
        override fun onDiscoveryStarted(serviceType: String) {
            stopped.set(false)
        }

        override fun onServiceFound(serviceInfo: NsdServiceInfo) {
            if (stopped.get() || serviceInfo.serviceType.trimEnd('.') != SERVICE_TYPE.trimEnd('.')) return
            synchronized(resolving) {
                if (!known.contains(serviceInfo.serviceName) && resolving.add(serviceInfo.serviceName)) {
                    try {
                        manager.resolveService(serviceInfo, resolveListener)
                    } catch (e: RuntimeException) {
                        resolving.remove(serviceInfo.serviceName)
                        notifyError("Falha ao resolver serviço local: ${e.message ?: "erro"}")
                    }
                }
            }
        }

        override fun onServiceLost(serviceInfo: NsdServiceInfo) {
            synchronized(resolving) { known.remove(serviceInfo.serviceName) }
            mainHandler.post { if (!stopped.get()) listener.onServerLost(serviceInfo.serviceName) }
        }

        override fun onDiscoveryStopped(serviceType: String) {
            stopped.set(true)
            requested.set(false)
        }

        override fun onStartDiscoveryFailed(serviceType: String, errorCode: Int) {
            stopped.set(true)
            requested.set(false)
            notifyError("Falha ao iniciar descoberta mDNS (código $errorCode)")
            try { manager.stopServiceDiscovery(this) } catch (_: RuntimeException) {}
        }

        override fun onStopDiscoveryFailed(serviceType: String, errorCode: Int) {
            notifyError("Falha ao encerrar descoberta mDNS (código $errorCode)")
            try { manager.stopServiceDiscovery(this) } catch (_: RuntimeException) {}
        }
    }

    private val resolveListener = object : NsdManager.ResolveListener {
        override fun onResolveFailed(serviceInfo: NsdServiceInfo, errorCode: Int) {
            synchronized(resolving) { resolving.remove(serviceInfo.serviceName) }
            notifyError("Não foi possível resolver servidor local (código $errorCode)")
        }

        override fun onServiceResolved(serviceInfo: NsdServiceInfo) {
            synchronized(resolving) { resolving.remove(serviceInfo.serviceName) }
            if (stopped.get()) return

            val host: InetAddress = serviceInfo.host ?: return
            // IPv4 is the v1 baseline. Ignore IPv6-only answers until the
            // complete transport and interface-selection path supports them.
            if (host !is Inet4Address) return
            val attrs = serviceInfo.attributes
            fun txt(key: String): String? = attrs[key]?.toString(Charsets.UTF_8)
            val version = txt("v") ?: return
            val installationId = txt("id") ?: return
            val state = txt("state") ?: return
            val capacity = txt("capacity")?.toIntOrNull() ?: return

            if (version != REQUIRED_PROTOCOL || !installationId.matches(Regex("[A-Fa-f0-9-]{16,64}")) ||
                state !in setOf("ready", "busy") || capacity !in 1..16 ||
                serviceInfo.port !in 1..65535) return

            synchronized(resolving) {
                if (!known.add(serviceInfo.serviceName)) return
            }
            val discovered = DiscoveredDbStudioServer(
                    serviceName = serviceInfo.serviceName,
                    hostAddress = host.hostAddress ?: return,
                    port = serviceInfo.port,
                    protocolVersion = version,
                    installationId = installationId,
                    state = state,
                    capacity = capacity
                )
            mainHandler.post {
                if (!stopped.get()) listener.onServerFound(discovered)
            }
        }
    }

    fun start() {
        if (!requested.compareAndSet(false, true)) return
        stopped.set(false)
        try {
            manager.discoverServices(SERVICE_TYPE, NsdManager.PROTOCOL_DNS_SD, discoveryListener)
        } catch (e: RuntimeException) {
            requested.set(false)
            stopped.set(true)
            notifyError("Descoberta mDNS indisponível: ${e.message ?: "erro"}")
        }
    }

    fun stop() {
        if (!requested.getAndSet(false)) return
        stopped.set(true)
        try { manager.stopServiceDiscovery(discoveryListener) } catch (_: RuntimeException) {}
        synchronized(resolving) {
            resolving.clear()
            known.clear()
        }
    }
}
