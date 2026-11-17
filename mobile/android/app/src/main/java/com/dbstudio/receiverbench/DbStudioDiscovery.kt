package com.dbstudio.receiverbench

import android.content.Context
import android.net.nsd.NsdManager
import android.net.nsd.NsdServiceInfo
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
                        listener.onDiscoveryError("Falha ao resolver serviço local: ${e.message ?: "erro"}")
                    }
                }
            }
        }

        override fun onServiceLost(serviceInfo: NsdServiceInfo) {
            synchronized(resolving) { known.remove(serviceInfo.serviceName) }
            listener.onServerLost(serviceInfo.serviceName)
        }

        override fun onDiscoveryStopped(serviceType: String) {
            stopped.set(true)
        }

        override fun onStartDiscoveryFailed(serviceType: String, errorCode: Int) {
            stopped.set(true)
            listener.onDiscoveryError("Falha ao iniciar descoberta mDNS (código $errorCode)")
            try { manager.stopServiceDiscovery(this) } catch (_: RuntimeException) {}
        }

        override fun onStopDiscoveryFailed(serviceType: String, errorCode: Int) {
            listener.onDiscoveryError("Falha ao encerrar descoberta mDNS (código $errorCode)")
            try { manager.stopServiceDiscovery(this) } catch (_: RuntimeException) {}
        }
    }

    private val resolveListener = object : NsdManager.ResolveListener {
        override fun onResolveFailed(serviceInfo: NsdServiceInfo, errorCode: Int) {
            synchronized(resolving) { resolving.remove(serviceInfo.serviceName) }
            if (!stopped.get()) listener.onDiscoveryError("Não foi possível resolver servidor local (código $errorCode)")
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
            listener.onServerFound(
                DiscoveredDbStudioServer(
                    serviceName = serviceInfo.serviceName,
                    hostAddress = host.hostAddress ?: return,
                    port = serviceInfo.port,
                    protocolVersion = version,
                    installationId = installationId,
                    state = state,
                    capacity = capacity
                )
            )
        }
    }

    fun start() {
        if (!stopped.get()) return
        try {
            manager.discoverServices(SERVICE_TYPE, NsdManager.PROTOCOL_DNS_SD, discoveryListener)
        } catch (e: RuntimeException) {
            listener.onDiscoveryError("Descoberta mDNS indisponível: ${e.message ?: "erro"}")
        }
    }

    fun stop() {
        if (stopped.getAndSet(true)) return
        try { manager.stopServiceDiscovery(discoveryListener) } catch (_: RuntimeException) {}
        synchronized(resolving) {
            resolving.clear()
            known.clear()
        }
    }
}
