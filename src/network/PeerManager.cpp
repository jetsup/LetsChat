#include "PeerManager.h"
#include "core/AppConfig.h"

PeerManager::PeerManager(QObject *parent)
    : QObject(parent)
    , m_discoveryService(new DiscoveryService(this))
    , m_peerModel(new PeerListModel(this))
    , m_databaseManager(new DatabaseManager(this))
    , m_isScanning(false)
{
    connect(m_discoveryService, &DiscoveryService::peerDiscovered, 
            this, &PeerManager::onPeerDiscovered);
    connect(m_discoveryService, &DiscoveryService::peerLost, 
            this, &PeerManager::onPeerLost);
    
    connect(m_discoveryService, &DiscoveryService::runningChanged, [this]() {
        m_isScanning = m_discoveryService->isRunning();
        emit scanningChanged();
    });
    
    m_databaseManager->initialize();
}

PeerListModel* PeerManager::peerModel() const
{
    return m_peerModel;
}

bool PeerManager::isScanning() const
{
    return m_isScanning;
}

void PeerManager::startScanning()
{
    m_discoveryService->startDiscovery();
}

void PeerManager::stopScanning()
{
    m_discoveryService->stopDiscovery();
}

void PeerManager::setLocalIdentity(const QString &userId, const QString &alias, 
                                  const QString &deviceName)
{
    m_localUserId = userId;
    m_discoveryService->setLocalInfo(userId, alias, deviceName);
}

void PeerManager::handleNetworkChange()
{
    // When joining a new LAN, mark all previous peers as offline
    // They will only be marked online again if discovered on this new LAN
    m_peerModel->markAllOffline();
    if (m_databaseManager) {
        m_databaseManager->markAllPeersOffline();
    }
    emit scanningChanged();
}

void PeerManager::onPeerDiscovered(const QString &userId, const QString &alias,
                                  const QString &deviceName, const QString &ipAddress)
{
    if (userId == m_localUserId) {
        return;
    }
    
    m_peerModel->addOrUpdatePeer(userId, alias, deviceName, ipAddress, true);
    if (m_databaseManager) {
        m_databaseManager->savePeer(userId, alias, deviceName, ipAddress, true);
    }
    emit peerConnected(userId);
}

void PeerManager::onPeerLost(const QString &userId)
{
    if (userId == m_localUserId) {
        return;
    }
    
    m_peerModel->markPeerOffline(userId);
    if (m_databaseManager) {
        m_databaseManager->updatePeerStatus(userId, false);
    }
    emit peerDisconnected(userId);
}
