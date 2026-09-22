type PortalServerMessage =
    | WelcomeMessage
    | MembershipMessage
    | JoinMessage
    | PartMessage
    | ChatMessage
    | AnnounceMessage

interface WelcomeMessage {
    type: "welcome";
    channels: string[];
}

interface MembershipMessage {
    type: "membership";
    channel: string;
    members: User[];
    history: ChatMessage[];
}

interface JoinMessage {
    type: "join";
    channel: string;
    user: User;
}

interface PartMessage {
    type: "part";
    channel: string;
    user: User;
}

interface ChatMessage {
    type: "chat";
    sender: string;
    text: string;
}

interface AnnounceMessage {
    type: "announce";
    sender: string;
    text: string;
}

type PortalClientRequest =
    | JoinRequest
    | PartRequest
    | SayRequest
    | AnnounceRequest
    ;

interface JoinRequest {
    type: "join";
    channel: string;
}
    
interface PartRequest {
    type: "part";
    channel: string;
}

interface SayRequest {
    type: "say";
    channel: string;
    text: string;
}

interface AnnounceRequest {
    type: "announce";
    text: string;
}

interface User {
    nick: string;
    address: string;
}
