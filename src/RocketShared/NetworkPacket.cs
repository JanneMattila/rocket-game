﻿using System;
using System.Collections.Generic;
using System.Linq;
using System.Net.Sockets;
using System.Net;
using System.Runtime.Intrinsics.Arm;
using System.Text;
using System.Threading.Tasks;
using System.IO.Hashing;

namespace RocketShared;

public class NetworkPacket
{
    public static byte[] ProtocolMagicNumber = [0xFE];
    public static int ExpectedMessageSize =
        sizeof(int) * 6  + /* pos(2) + vel(2) + rotation + speed */
        sizeof(byte) + /* keyboard */
        sizeof(int); /* crc32 */
    public float PositionX { get; set; }
    public float PositionY { get; set; }
    public float VelocityX { get; set; }
    public float VelocityY { get; set; }
    public float Rotation { get; set; }
    public float Speed { get; set; }
    public byte IsUp { get; set; }
    public byte IsDown { get; set; }
    public byte IsLeft { get; set; }
    public byte IsRight { get; set; }
    public byte IsFiring { get; set; }

    public byte[] GetBytes(System.IO.Hashing.Crc32 crc32)
    {
        var data = new byte[ExpectedMessageSize];
        var offset = crc32.HashLengthInBytes;

        WriteFloatToInt(data, ref offset, PositionX);
        WriteFloatToInt(data, ref offset, PositionY);
        WriteFloatToInt(data, ref offset, VelocityX);
        WriteFloatToInt(data, ref offset, VelocityY);
        WriteFloatToInt(data, ref offset, Rotation);
        WriteFloatToInt(data, ref offset, Speed);
        data[offset++] = (byte)(
            (IsUp << 5) |
            (IsDown << 4) |
            (IsLeft << 3) |
            (IsRight << 2) |
            IsFiring);

        crc32.Reset();
        crc32.Append(ProtocolMagicNumber);
        crc32.Append(new ReadOnlySpan<byte>(data, crc32.HashLengthInBytes, data.Length - crc32.HashLengthInBytes));

        var crc32value = crc32.GetCurrentHash();
        Buffer.BlockCopy(crc32value, 0, data, 0, crc32.HashLengthInBytes);
        return data;
    }

    public static NetworkPacket FromBytes(System.IO.Hashing.Crc32 crc32, byte[] data)
    {
        var offset = crc32.HashLengthInBytes;

        if (data.Length != ExpectedMessageSize)
        {
            throw new ApplicationException($"Invalid message size. Expected: {ExpectedMessageSize} vs. Actual: {data.Length}");
        }

        crc32.Reset();
        crc32.Append(ProtocolMagicNumber);
        crc32.Append(new ReadOnlySpan<byte>(data, crc32.HashLengthInBytes, data.Length - crc32.HashLengthInBytes));
        var crc32valueReceived = BitConverter.ToInt32(data);
        var crc32valueCalculated = BitConverter.ToInt32(crc32.GetCurrentHash());

        if (crc32valueReceived != crc32valueCalculated)
        {
            throw new ApplicationException($"Received CRC32 value {crc32valueReceived}, calculated {crc32valueCalculated}.");
        }

        var positionX = ReadIntToFloat(data, ref offset);
        var positionY = ReadIntToFloat(data, ref offset);
        var velocityX = ReadIntToFloat(data, ref offset);
        var velocityY = ReadIntToFloat(data, ref offset);
        var rotation = ReadIntToFloat(data, ref offset);
        var speed = ReadIntToFloat(data, ref offset);
        var keyboard = data[offset++];

        var isUp = (byte)((keyboard >> 5) & 1);
        var isDown = (byte)((keyboard >> 4) & 1);
        var isLeft = (byte)((keyboard >> 3) & 1);
        var isRight = (byte)((keyboard >> 2) & 1);
        var isFiring = (byte)(keyboard & 1);

        return new NetworkPacket()
        {
            PositionX = positionX,
            PositionY = positionY,
            VelocityX = velocityX,
            VelocityY = velocityY,
            Rotation = rotation,
            Speed = speed,
            IsUp = isUp,
            IsDown = isDown,
            IsLeft = isLeft,
            IsRight = isRight,
            IsFiring = isFiring
        };
    }

    private void WriteFloatToInt(byte[] data, ref int offset, float value)
    {
        var convertedValue = BitConverter.GetBytes(IPAddress.HostToNetworkOrder((int)(value * 1_000)));
        Buffer.BlockCopy(convertedValue, 0, data, offset, sizeof(int));
        offset += sizeof(int);
    }

    private static float ReadIntToFloat(byte[] data, ref int offset)
    {
        var value = BitConverter.ToInt32(data, offset);
        offset += sizeof(int);
        return IPAddress.NetworkToHostOrder(value) / 1_000f;
    }
}
