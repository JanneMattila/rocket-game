using Microsoft.Extensions.Configuration;
using RocketShared;
using System.Diagnostics;
using System.Net;
using System.Net.Sockets;

var builder = new ConfigurationBuilder()
    .AddUserSecrets<Program>()
    .AddJsonFile("appsettings.json", optional: true);

var configuration = builder.Build();

var udpPort = configuration.GetValue<int>("udpPort", 3501);

Console.WriteLine($"UDP Port: {udpPort}");

var players = new Dictionary<IPEndPoint, RocketPlayer>();
var listener = new UdpClient(udpPort);
var remoteEndpoint = new IPEndPoint(IPAddress.Any, udpPort);

System.IO.Hashing.Crc32 crc32 = new();
var protocolMagicNumber = new ReadOnlySpan<byte>(BitConverter.GetBytes((short)0xFE));

var stopwatch = new Stopwatch();
stopwatch.Start();

while (true)
{
    byte[] receivedBytes;

    try
    {
        receivedBytes = listener.Receive(ref remoteEndpoint);
    }
    catch (SocketException ex)
    {
        Debug.WriteLine("SocketException caught: {0}", ex.Message);
        continue;
    }

    var packet = NetworkPacket.FromBytes(crc32, receivedBytes);

    var now = DateTime.UtcNow;
    RocketPlayer? rocketPlayer;
    if (!players.TryGetValue(remoteEndpoint, out rocketPlayer))
    {
        rocketPlayer = new RocketPlayer
        {
            PlayerID = (byte)(players.Count + 1),
            Address = remoteEndpoint,
            Created = now
        };
        players.Add(remoteEndpoint, rocketPlayer);
    }

    rocketPlayer.PositionX = packet.PositionX;
    rocketPlayer.PositionY = packet.PositionY;
    rocketPlayer.VelocityX = packet.VelocityX;
    rocketPlayer.VelocityY = packet.VelocityY;
    rocketPlayer.Rotation = packet.Rotation;
    rocketPlayer.Speed = packet.Speed;
    rocketPlayer.IsFiring = packet.IsFiring;

    rocketPlayer.LastUpdated = now;

    // TODO: Add received sequence number to received items list.

    foreach (var b in players)
    {
        if (b.Value.PlayerID == rocketPlayer.PlayerID) continue;
        listener.Send(receivedBytes, receivedBytes.Length, b.Key);
    }

    rocketPlayer.Messages++;

    if (stopwatch.ElapsedMilliseconds >= 1_000)
    {
        var lastUpdateThreshold = now.AddSeconds(-5);

        //Console.WriteLine($"{players.Count} clients:");
        var toRemove = new List<IPEndPoint>();
        foreach (var b in players)
        {
            //Console.WriteLine($"{b.Key}: {b.Value.Messages} packets, PositionX: {b.Value.PositionX}, PositionY: {b.Value.PositionY}");
            if (b.Value.LastUpdated < lastUpdateThreshold)
            {
                toRemove.Add(b.Key);
            }

            b.Value.Messages = 0;
        }

        foreach (var r in toRemove)
        {
            players.Remove(r);
        }
        stopwatch.Restart();
    }
}

class RocketPlayer
{
    public byte PlayerID { get; set; }
    public byte SequenceNumber { get; set; }
    public IPEndPoint? Address { get; set; }
    public DateTime Created { get; set; }
    public DateTime LastUpdated { get; set; }
    public int Messages { get; set; }

    public float PositionX { get; set; }
    public float PositionY { get; set; }
    public float VelocityX { get; set; }
    public float VelocityY { get; set; }
    public float Rotation { get; set; }
    public float Speed { get; set; }
    public byte IsFiring { get; set; }
}