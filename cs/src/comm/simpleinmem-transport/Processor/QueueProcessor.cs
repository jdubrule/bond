// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

namespace Bond.Comm.SimpleInMem.Processor
{
    using System;
    using System.Threading;
    using System.Threading.Tasks.Dataflow;
    using Bond.Comm.Service;

    internal abstract class QueueProcessor
    {
        private const int MIN_DELAY_MILLISEC = 1;
        private const int MAX_DELAY_MILLISEC = 11;
        private readonly object processInput = new object();
        protected readonly SimpleInMemConnection connection;
        protected readonly ServiceHost serviceHost;

        internal QueueProcessor(SimpleInMemConnection connection, ServiceHost serviceHost)
        {
            if (connection == null) throw new ArgumentNullException(nameof(connection));
            if (serviceHost == null) throw new ArgumentNullException(nameof(serviceHost));

            this.connection = connection;
            this.serviceHost = serviceHost;
        }

        public void ProcessAsync(CancellationToken t)
        {
            Util.CreateLongRunningTask(o => Process(), t, MIN_DELAY_MILLISEC, MAX_DELAY_MILLISEC).Post(processInput);
        }

        internal abstract void Process();
    }
}
