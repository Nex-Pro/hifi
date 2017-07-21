//
//  PacketHeaders.cpp
//  libraries/networking/src
//
//  Created by Stephen Birarda on 6/28/13.
//  Copyright 2013 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "PacketHeaders.h"

#include <math.h>
#include <mutex>

#include <QtCore/QDataStream>
#include <QtCore/QDebug>
#include <QtCore/QMetaEnum>


Q_DECLARE_METATYPE(PacketType);
int packetTypeMetaTypeId = qRegisterMetaType<PacketType>();

const QSet<PacketType> NON_VERIFIED_PACKETS = QSet<PacketType>()
    << PacketType::NodeJsonStats << PacketType::EntityQuery
    << PacketType::OctreeDataNack << PacketType::EntityEditNack
    << PacketType::DomainListRequest << PacketType::StopNode
    << PacketType::DomainDisconnectRequest << PacketType::UsernameFromIDRequest
    << PacketType::NodeKickRequest << PacketType::NodeMuteRequest;

const QSet<PacketType> NON_SOURCED_PACKETS = QSet<PacketType>()
    << PacketType::StunResponse << PacketType::CreateAssignment << PacketType::RequestAssignment
    << PacketType::DomainServerRequireDTLS << PacketType::DomainConnectRequest
    << PacketType::DomainList << PacketType::DomainConnectionDenied
    << PacketType::DomainServerPathQuery << PacketType::DomainServerPathResponse
    << PacketType::DomainServerAddedNode << PacketType::DomainServerConnectionToken
    << PacketType::DomainSettingsRequest << PacketType::DomainSettings
    << PacketType::ICEServerPeerInformation << PacketType::ICEServerQuery << PacketType::ICEServerHeartbeat
    << PacketType::ICEServerHeartbeatACK << PacketType::ICEPing << PacketType::ICEPingReply
    << PacketType::ICEServerHeartbeatDenied << PacketType::AssignmentClientStatus << PacketType::StopNode
    << PacketType::DomainServerRemovedNode << PacketType::UsernameFromIDReply << PacketType::OctreeFileReplacement
    << PacketType::ReplicatedMicrophoneAudioNoEcho << PacketType::ReplicatedMicrophoneAudioWithEcho
    << PacketType::ReplicatedInjectAudio << PacketType::ReplicatedSilentAudioFrame
    << PacketType::ReplicatedAvatarIdentity << PacketType::ReplicatedKillAvatar << PacketType::ReplicatedBulkAvatarData
    << PacketType::OctreeFileReplacementFromUrl;

const QHash<PacketType, PacketType> REPLICATED_PACKET_MAPPING {
    { PacketType::MicrophoneAudioNoEcho, PacketType::ReplicatedMicrophoneAudioNoEcho },
    { PacketType::MicrophoneAudioWithEcho, PacketType::ReplicatedMicrophoneAudioWithEcho },
    { PacketType::InjectAudio, PacketType::ReplicatedInjectAudio },
    { PacketType::SilentAudioFrame, PacketType::ReplicatedSilentAudioFrame },
    { PacketType::AvatarIdentity, PacketType::ReplicatedAvatarIdentity },
    { PacketType::KillAvatar, PacketType::ReplicatedKillAvatar },
    { PacketType::BulkAvatarData, PacketType::ReplicatedBulkAvatarData }
};

PacketVersion versionForPacketType(PacketType packetType) {
    switch (packetType) {
        case PacketType::DomainList:
            return static_cast<PacketVersion>(DomainListVersion::GetMachineFingerprintFromUUIDSupport);
        case PacketType::EntityAdd:
        case PacketType::EntityEdit:
        case PacketType::EntityData:
        case PacketType::EntityPhysics:
            return VERSION_ENTITIES_BULLET_DYNAMICS;
        case PacketType::EntityQuery:
            return static_cast<PacketVersion>(EntityQueryPacketVersion::JSONFilterWithFamilyTree);
        case PacketType::AvatarIdentity:
        case PacketType::AvatarData:
        case PacketType::BulkAvatarData:
        case PacketType::KillAvatar:
            return static_cast<PacketVersion>(AvatarMixerPacketVersion::IsReplicatedInAvatarIdentity);
        case PacketType::MessagesData:
            return static_cast<PacketVersion>(MessageDataVersion::TextOrBinaryData);
        case PacketType::ICEServerHeartbeat:
            return 18; // ICE Server Heartbeat signing
        case PacketType::AssetGetInfo:
        case PacketType::AssetGet:
        case PacketType::AssetUpload:
            return static_cast<PacketVersion>(AssetServerPacketVersion::RangeRequestSupport);
        case PacketType::NodeIgnoreRequest:
            return 18; // Introduction of node ignore request (which replaced an unused packet tpye)

        case PacketType::DomainConnectionDenied:
            return static_cast<PacketVersion>(DomainConnectionDeniedVersion::IncludesExtraInfo);

        case PacketType::DomainConnectRequest:
            return static_cast<PacketVersion>(DomainConnectRequestVersion::HasMachineFingerprint);

        case PacketType::DomainServerAddedNode:
            return static_cast<PacketVersion>(DomainServerAddedNodeVersion::PermissionsGrid);

        case PacketType::MixedAudio:
        case PacketType::SilentAudioFrame:
        case PacketType::InjectAudio:
        case PacketType::MicrophoneAudioNoEcho:
        case PacketType::MicrophoneAudioWithEcho:
        case PacketType::AudioStreamStats:
            return static_cast<PacketVersion>(AudioVersion::HighDynamicRangeVolume);

        default:
            return 17;
    }
}

uint qHash(const PacketType& key, uint seed) {
    // seems odd that Qt couldn't figure out this cast itself, but this fixes a compile error after switch
    // to strongly typed enum for PacketType
    return qHash((quint8) key, seed);
}

QDebug operator<<(QDebug debug, const PacketType& type) {
    QMetaObject metaObject = PacketTypeEnum::staticMetaObject;
    QMetaEnum metaEnum = metaObject.enumerator(metaObject.enumeratorOffset());
    QString typeName = metaEnum.valueToKey((int) type);

    debug.nospace().noquote() << (uint8_t) type << " (" << typeName << ")";
    return debug.space();
}

#if (PR_BUILD || DEV_BUILD)
static bool sendWrongProtocolVersion = false;
void sendWrongProtocolVersionsSignature(bool sendWrongVersion) {
    sendWrongProtocolVersion = sendWrongVersion;
}
#endif

static QByteArray protocolVersionSignature;
static QString protocolVersionSignatureBase64;
static void ensureProtocolVersionsSignature() {
    static std::once_flag once;
    std::call_once(once, [&] {
        QByteArray buffer;
        QDataStream stream(&buffer, QIODevice::WriteOnly);
        uint8_t numberOfProtocols = static_cast<uint8_t>(PacketType::NUM_PACKET_TYPE);
        stream << numberOfProtocols;
        for (uint8_t packetType = 0; packetType < numberOfProtocols; packetType++) {
            uint8_t packetTypeVersion = static_cast<uint8_t>(versionForPacketType(static_cast<PacketType>(packetType)));
            stream << packetTypeVersion;
        }
        QCryptographicHash hash(QCryptographicHash::Md5);
        hash.addData(buffer);
        protocolVersionSignature = hash.result();
        protocolVersionSignatureBase64 = protocolVersionSignature.toBase64();
    });
}
QByteArray protocolVersionsSignature() {
    ensureProtocolVersionsSignature();
    #if (PR_BUILD || DEV_BUILD)
    if (sendWrongProtocolVersion) {
        return QByteArray("INCORRECTVERSION"); // only for debugging version checking
    }
    #endif

    return protocolVersionSignature;
}
QString protocolVersionsSignatureBase64() {
    ensureProtocolVersionsSignature();
    return protocolVersionSignatureBase64;
}
