﻿// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

namespace Bond.Comm.Epoxy
{
    using System;
    using System.Diagnostics;
    using System.IO;
    using System.Net;
    using System.Net.Security;
    using System.Net.Sockets;
    using System.Security.Authentication;
    using System.Security.Cryptography.X509Certificates;
    using System.Threading;
    using System.Threading.Tasks;

    /// <summary>
    /// Epoxy-private wrapper around <see cref="Socket"/>. Provides idempotent shutdown.
    /// </summary>
    internal class EpoxyNetworkStream
    {
        const SslProtocols AllowedTlsProtocols = SslProtocols.Tls12;

        Socket socket;
        Stream stream;

        // Caches of the values from the underlying socket so we can use them after socket
        // shutdown.
        public IPEndPoint LocalEndpoint { get; }
        public IPEndPoint RemoteEndpoint { get; }

        // It looks like we don't need to .Dispose this SemaphoreSlim. The
        // current implementation of SemaphoreSlim only does interesting
        // stuff during .Dispose if there's an allocated
        // AvailableWaitHandle. We never call that, so there shouldn't be
        // anything needing disposal. If we do end up allocating a wait
        // handle somehow, its finalizer will save us.
        public SemaphoreSlim WriteLock { get; }

        int isShutdown;
        Logger logger;

        private EpoxyNetworkStream(Socket socket, Stream stream, Logger logger)
        {
            Debug.Assert(socket != null);
            Debug.Assert(stream != null);
            Debug.Assert(logger != null);

            this.socket = socket;
            this.stream = stream;

            LocalEndpoint = (IPEndPoint)socket.LocalEndPoint;
            RemoteEndpoint = (IPEndPoint)socket.RemoteEndPoint;

            WriteLock = new SemaphoreSlim(1, 1);
            isShutdown = 0;

            this.logger = logger;
        }

        public static async Task<EpoxyNetworkStream> MakeClientStreamAsync(
            string remoteHostname,
            Socket socket,
            EpoxyClientTlsConfig tlsConfig,
            Logger logger)
        {
            Stream clientStream;
            NetworkStream networkStream = new NetworkStream(socket, ownsSocket: false);

            if (tlsConfig == null)
            {
                clientStream = networkStream;
            }
            else
            {
                const bool leaveInnerStreamOpen = false;

                var sslStream = new SslStream(
                    networkStream,
                    leaveInnerStreamOpen,
                    tlsConfig.RemoteCertificateValidationCallback);

                var clientCertificates = new X509CertificateCollection();
                if (tlsConfig.Certificate != null)
                {
                    clientCertificates.Add(tlsConfig.Certificate);
                }

                await sslStream.AuthenticateAsClientAsync(
                    remoteHostname,
                    clientCertificates,
                    AllowedTlsProtocols,
                    tlsConfig.CheckCertificateRevocation);

                logger.Site().Debug("Authenticated connection to {0}[{1}]", remoteHostname, socket.RemoteEndPoint);

                clientStream = sslStream;
            }

            return new EpoxyNetworkStream(socket, clientStream, logger);
        }

        public static async Task<EpoxyNetworkStream> MakeServerStreamAsync(
            Socket socket,
            EpoxyServerTlsConfig tlsConfig,
            Logger logger)
        {
            Stream serverStream;
            var networkStream = new NetworkStream(socket, ownsSocket: false);

            if (tlsConfig == null)
            {
                serverStream = networkStream;
            }
            else
            {
                const bool leaveInnerStreamOpen = false;

                var sslStream = new SslStream(
                    networkStream,
                    leaveInnerStreamOpen,
                    MakeServerCertificateValidationCallback(tlsConfig, logger));

                await sslStream.AuthenticateAsServerAsync(
                    tlsConfig.Certificate,
                    tlsConfig.ClientCertificateRequired,
                    enabledSslProtocols: AllowedTlsProtocols,
                    checkCertificateRevocation: tlsConfig.CheckCertificateRevocation);

                if (tlsConfig.ClientCertificateRequired && !sslStream.IsMutuallyAuthenticated)
                {
                    sslStream.Dispose();
                    throw new AuthenticationException("Mutual authentication was required, but it could not be performed.");
                }

                logger.Site().Debug(
                    "Authenticated connection from {0}. Mutually authenticated?: {1}",
                    socket.RemoteEndPoint,
                    sslStream.IsMutuallyAuthenticated);

                serverStream = sslStream;
            }

            return new EpoxyNetworkStream(socket, serverStream, logger);
        }

        public Stream Stream
        {
            get
            {
                if (isShutdown != 0)
                {
                    throw new ObjectDisposedException(nameof(EpoxyNetworkStream));
                }

                return stream;
            }
        }

        public void Shutdown()
        {
            int oldIsShutdown = Interlocked.CompareExchange(ref isShutdown, 1, 0);
            if (oldIsShutdown == 0)
            {
                // we are responsible for shutdown
                try
                {
                    stream.Dispose();

                    try
                    {
                        socket.Shutdown(SocketShutdown.Send);
                        socket.Close();
                    }
                    catch (ObjectDisposedException)
                    {
                        // ignore, as we're shutting down anyway
                    }

                    // We cannot call socket.Disconnect, as that will block
                    // for longer than we want. So, we just forcible close
                    // the socket with Dispose
                    socket.Dispose();
                }
                catch (Exception ex) when (ex is IOException || ex is SocketException)
                {
                    logger.Site().Error(ex, "Exception during connection shutdown");
                }

                stream = null;
                socket = null;
                logger = null;
            }
        }

        private static RemoteCertificateValidationCallback MakeServerCertificateValidationCallback(
            EpoxyServerTlsConfig tlsConfig,
            Logger logger)
        {
            if (tlsConfig.ClientCertificateRequired)
            {
                // If client certificates are required, then add an explicit
                // check that the client provided a certificate. The default
                // behavior is to allow the connection even if the client
                // didn't present a certificate.
                return (sender, certificate, chain, errors) =>
                {
                    if (certificate == null)
                    {
                        logger.Site().Error("Rejecting client. Certificate required, but client did not provide one.");
                        return false;
                    }

                    if (tlsConfig.RemoteCertificateValidationCallback != null)
                    {
                        // There's a user-provided validation callback, so
                        // delegate to that.
                        return tlsConfig.RemoteCertificateValidationCallback(
                            sender,
                            certificate,
                            chain,
                            errors);
                    }
                    else
                    {
                        // Otherwise, require no errors at all to accept the
                        // certificate.
                        return errors == SslPolicyErrors.None;
                    }
                };
            }
            else
            {
                // Client certificates are not required, so just use the
                // user-provided validation callback. This may be null, but
                // that's fine. SslStream will just use its default behavior
                // then.
                return tlsConfig.RemoteCertificateValidationCallback;
            }
        }
    }
}
