const int DZT_RPC_REQUEST = 784231;
const int DZT_RPC_RESPONSE = 784232;
const int DZT_RPC_NOTIFY = 784233;
const int DZT_MENU = 784234;
const int DZT_NET_MAGIC = 1146762290;
const int DZT_NET_VERSION = 5;

class DZTRequest
{
    string op;
    string request_id;
    int ticket_id;
    int event_id;
    int before;
    bool all_open;
    string owner;
    string category;
    string body;
    bool confirmed;
}

class DZTTicket
{
    int id;
    string server;
    string server_name;
    string steam;
    string name;
    string category;
    string position;
    string created;
    string status;
    string assignee;
}

class DZTMessage
{
    int id;
    int ticket;
    string actor;
    string actor_name;
    string kind;
    string body;
    string created;
    string steam;
}

class DZTResponse
{
    bool ok;
    bool admin;
    string op;
    string request_id;
    string error;
    int ticket_id;
    bool has_more;
    int next_before;
    ref array<string> categories = new array<string>;
    ref array<ref DZTTicket> tickets = new array<ref DZTTicket>;
    ref array<ref DZTMessage> messages = new array<ref DZTMessage>;
    ref array<ref DZTMessage> notifications = new array<ref DZTMessage>;
}

class DZTFrame
{
    int nonce;
    int id;
    int index;
    int count;
    int bytes;
    string data;
}

class DZTAssembly
{
    string peer;
    int started;
    int count;
    int bytes;
    int received;
    int receivedBytes;
    ref array<string> parts = new array<string>;
    ref array<bool> present = new array<bool>;
}

class DZTBus
{
    static ref ScriptInvoker Server = new ScriptInvoker;
    static ref ScriptInvoker Client = new ScriptInvoker;
    static ref map<string, ref DZTAssembly> pending = new map<string, ref DZTAssembly>;
    static int sessionNonce;
    static int nextMessage;
    static int lastFailure;

    static bool Fail(int code) { lastFailure = code; return false; }

    static bool WriteFrame(Serializer context, DZTFrame frame)
    {
        if (!context.Write(DZT_NET_MAGIC) || !context.Write(DZT_NET_VERSION) || !context.Write(frame.nonce) || !context.Write(frame.id)) return false;
        if (!context.Write(frame.index) || !context.Write(frame.count) || !context.Write(frame.bytes)) return false;
        int length = frame.data.Length();
        if (length > 128 || !context.Write(length)) return false;
        // Encode UTF-8 as bounded byte values, not a native variable-length string.
        for (int i = 0; i < length; i++)
        {
            int value = frame.data.Substring(i, 1).ToAscii() & 255;
            if (!context.Write(value)) return false;
        }
        return true;
    }

    static bool ReceivePacket(Serializer context, int rpcType, string peer, out string json, out bool complete)
    {
        lastFailure = 0;
        json = "";
        complete = false;
        int magic, version;
        if (!context.Read(magic) || magic != DZT_NET_MAGIC) return Fail(1);
        if (!context.Read(version) || version != DZT_NET_VERSION) return Fail(2);
        DZTFrame frame = new DZTFrame;
        frame.data = "";
        if (!context.Read(frame.nonce) || !context.Read(frame.id) || !context.Read(frame.index)) return Fail(3);
        if (!context.Read(frame.count) || !context.Read(frame.bytes)) return Fail(4);
        int length;
        if (!context.Read(length) || length < 0 || length > 128) return Fail(5);
        for (int byteIndex = 0; byteIndex < length; byteIndex++)
        {
            int value;
            if (!context.Read(value) || value < 0 || value > 255) return Fail(6);
            frame.data += value.AsciiToString();
        }
        int maximum = 150000;
        if (rpcType == DZT_RPC_REQUEST) maximum = 12000;
        if (frame.nonce <= 0 || frame.id <= 0 || frame.bytes < 0 || frame.bytes > maximum) return Fail(7);
        if (frame.count < 1 || frame.count > 4800 || frame.count > frame.bytes + 1) return Fail(8);
        if (frame.index < 0 || frame.index >= frame.count) return Fail(9);
        if (frame.data.LengthUtf8() > 32 || frame.data.Length() > 128) return Fail(10);
        int now = GetGame().GetTime();
        int peerPending = 0;
        for (int i = pending.Count() - 1; i >= 0; i--)
        {
            string oldKey = pending.GetKey(i);
            DZTAssembly old = pending.GetElement(i);
            int age = now - old.started;
            if (age < 0 || age > 15000) pending.Remove(oldKey);
            else if (old.peer == peer) peerPending++;
        }
        string key = rpcType.ToString() + ":" + peer + ":" + frame.nonce.ToString() + ":" + frame.id.ToString();
        DZTAssembly assembly;
        if (!pending.Find(key, assembly))
        {
            if (pending.Count() >= 128 || peerPending >= 4) return Fail(11);
            assembly = new DZTAssembly;
            assembly.peer = peer;
            assembly.started = now;
            assembly.count = frame.count;
            assembly.bytes = frame.bytes;
            assembly.received = 0;
            assembly.receivedBytes = 0;
            assembly.parts.Resize(frame.count);
            assembly.present.Resize(frame.count);
            for (int fill = 0; fill < frame.count; fill++) { assembly.present[fill] = false; assembly.parts[fill] = ""; }
            pending.Set(key, assembly);
        }
        if (assembly.count != frame.count || assembly.bytes != frame.bytes)
        {
            pending.Remove(key);
            return Fail(12);
        }
        if (assembly.present[frame.index])
        {
            if (assembly.parts[frame.index] != frame.data) return Fail(99);
            return true;
        }
        assembly.parts[frame.index] = frame.data;
        assembly.present[frame.index] = true;
        assembly.received++;
        assembly.receivedBytes += frame.data.Length();
        if (assembly.receivedBytes > assembly.bytes)
        {
            pending.Remove(key);
            return Fail(13);
        }
        if (assembly.received < assembly.count) return true;
        for (int part = 0; part < assembly.count; part++) json += assembly.parts[part];
        pending.Remove(key);
        if (json.Length() != assembly.bytes) { json = ""; return Fail(14); }
        complete = true;
        return true;
    }

    static void SendPacket(int rpcType, string json, PlayerIdentity recipient = null)
    {
        int maximum = 150000;
        if (rpcType == DZT_RPC_REQUEST) maximum = 12000;
        if (json.Length() > maximum) return;
        if (sessionNonce == 0) sessionNonce = Math.RandomInt(1, 2147483646);
        if (nextMessage >= 2147483646) nextMessage = 0;
        nextMessage++;
        int characters = json.LengthUtf8();
        int count = Math.Max(1, (characters + 31) / 32);
        for (int index = 0; index < count; index++)
        {
            DZTFrame frame = new DZTFrame;
            frame.nonce = sessionNonce;
            frame.id = nextMessage;
            frame.index = index;
            frame.count = count;
            frame.bytes = json.Length();
            int offset = index * 32;
            frame.data = json.SubstringUtf8(offset, Math.Min(32, characters - offset));
            ScriptRPC packet = new ScriptRPC;
            if (!WriteFrame(packet, frame))
            {
                Print("[DZTickets] Network frame serialization failed.");
                return;
            }
            packet.Send(null, rpcType, true, recipient);
        }
    }

    static void Send(DZTRequest request)
    {
        string json, error;
        if (JsonFileLoader<DZTRequest>.MakeData(request, json, error, false))
            SendPacket(DZT_RPC_REQUEST, json);
    }
}

modded class DayZGame
{
    override void OnRPC(PlayerIdentity sender, Object target, int rpc_type, ParamsReadContext ctx)
    {
        if (rpc_type != DZT_RPC_REQUEST && rpc_type != DZT_RPC_RESPONSE && rpc_type != DZT_RPC_NOTIFY)
        {
            super.OnRPC(sender, target, rpc_type, ctx);
            return;
        }
        // Check direction before reading any payload. Do not deserialize a reflected
        // Param1 object. Every bounded frame is read as primitive fields.
        if (IsServer() && (rpc_type != DZT_RPC_REQUEST || !sender)) return;
        if (!IsServer() && rpc_type == DZT_RPC_REQUEST) return;
        string payload;
        bool complete;
        string peer = "server";
        if (sender) peer = sender.GetPlainId();
        if (!DZTBus.ReceivePacket(ctx, rpc_type, peer, payload, complete))
        {
            Print("[DZTickets] Rejected incompatible or malformed network packet: " + rpc_type.ToString());
            return;
        }
        if (!complete) return;
        if (IsServer() && payload.Length() <= 12000)
            DZTBus.Server.Invoke(sender, payload);
        else if (IsClient())
            DZTBus.Client.Invoke(rpc_type, payload);
    }
}

