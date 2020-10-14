/*
* Copyright (C) 2019 ~ 2020 Uniontech Software Technology Co.,Ltd
*
* Author:      maojj <maojunjie@uniontech.com>
* Maintainer:  maojj <maojunjie@uniontech.com>
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef NETIF_MONITOR_H
#define NETIF_MONITOR_H

#include "netif_packet_parser.h"

#include <QThread>
#include <QSharedPointer>
#include <QMap>
#include <QMutex>
#include <QWaitCondition>
#include <QMutexLocker>

class NetifMonitorJob;

/**
 * @brief Network interface socket io stat structure
 */
struct sock_io_stat_t {
    ino_t ino; // socket inode
    qulonglong rx_bytes; // received bytes
    qulonglong rx_packets; // received packets
    qulonglong tx_bytes; // sent bytes
    qulonglong tx_packets; // sent packets
};

// socket io stat typedef
using SockIOStat = QSharedPointer<struct sock_io_stat_t>;

/**
 * @brief Network interface monitor class
 */
class NetifMonitor : public QThread
{
    Q_OBJECT

public:
    /**
     * @brief Monitor constructor
     * @param parent Parent object
     */
    explicit NetifMonitor(QObject *parent = nullptr);
    /**
    * @brief Destructor
    */
    ~NetifMonitor() override;

    /**
     * @brief
     */
    inline void requestQuit()
    {
        m_quitRequested.store(true);
        m_pktqWatcher.wakeAll();
    }

    /**
     * @brief Get socket io stat data with specified inode (thread safe accessor)
     * @param ino Socket inode
     * @param stat Socket io stat data
     * @return Return true if stat data found in stat cache, otherwise return false
     */
    inline bool getSockIOStatByInode(ino_t ino, SockIOStat &stat)
    {
        bool ok = false;

        // lock cache before access
        m_sockIOStatMapLock.lock();

        // check stat data existence
        if (m_sockIOStatMap.contains(ino)) {
            stat = m_sockIOStatMap[ino];
            // remove stat data from cache
            m_sockIOStatMap.remove(ino);
            ok = true;
        }

        m_sockIOStatMapLock.unlock();

        return ok;
    }

    /**
     * @brief Run packet monitor job in monitor thread
     */
    void run() override;

public Q_SLOTS:

private:
    // pending packet queue
    PacketPayloadQueue  m_pendingPackets        {};
    // pending packet queue locker
    QMutex              m_pktqLock              {};
    // packet queue watcher
    QWaitCondition      m_pktqWatcher           {};
    // local pending packet queue
    PacketPayloadQueue  m_localPendingPackets   {}; // local cache

    // socket inode to io stat mapping
    QMap<ino_t, SockIOStat> m_sockIOStatMap     {};
    // socket io stat map access locker
    QMutex                  m_sockIOStatMapLock {};

    // packet monitor thread object
    QThread             m_packetMonitorThread;
    // packet monitor job instace
    NetifMonitorJob    *m_packetMonitorJob     {};

    // quit atomic test flag
    std::atomic_bool m_quitRequested {false};

    friend void pcap_callback(u_char *, const struct pcap_pkthdr *, const u_char *);
    friend class NetifMonitorJob;
};

#endif // NETIF_MONITOR_H
