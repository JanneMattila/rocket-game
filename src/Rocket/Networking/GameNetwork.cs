using RocketShared;
using System;
using System.Collections.Concurrent;
using System.Diagnostics;
using System.Net;
using System.Net.Sockets;

namespace Rocket.Networking;

public class GameNetwork(int udpPort, string server)
{
    private readonly int _udpPort = udpPort;
    private readonly string _server = server;
    private UdpClient _client;

    public static GameNetworkStatus Status { get; set; } = GameNetworkStatus.Connecting;
    public static ConcurrentQueue<NetworkPacket> Incoming { get; } = new();
    public static ConcurrentQueue<NetworkPacket> Outgoing { get; } = new();

    public void RunNetworking()
    {
        _client = new UdpClient(_server, _udpPort)
        {
            DontFragment = true
        };
        _client.AllowNatTraversal(true);
        var serverEndpoint = new IPEndPoint(IPAddress.Parse(_server), _udpPort);

        System.IO.Hashing.Crc32 crc32 = new();
        var lastUpdate = DateTime.Now.Ticks;
        Status = GameNetworkStatus.Connected;

        while (Status != GameNetworkStatus.Disconnected)
        {
            var now = DateTime.Now.Ticks;
            var delta = (now - lastUpdate) / (double)TimeSpan.TicksPerSecond;
            lastUpdate = now;

            if (Outgoing.TryDequeue(out var packet))
            {
                var data = packet.GetBytes(crc32);
                _client.Send(data, data.Length);
            }

            while (_client.Available > 1)
            {
                try
                {
                    var receivedBytes = _client.Receive(ref serverEndpoint);
                    var packetIncoming = NetworkPacket.FromBytes(crc32, receivedBytes);
                    Incoming.Enqueue(packetIncoming);
                }
                catch (SocketException ex)
                {
                    Debug.WriteLine("SocketException caught: {0}", ex.Message);
                    Status = GameNetworkStatus.Disconnected;
                }
            }
        }

        _client.Close();
    }
}
