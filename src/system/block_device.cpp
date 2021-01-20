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

#include "block_device.h"
#include <QDateTime>
#include <QFile>
#include <QSharedData>
#include <QTextStream>
#include "common/common.h"
namespace core {
namespace system {

BlockDevice::BlockDevice()
    : d(new BlockDevicePrivate())
{
}
BlockDevice::BlockDevice(const BlockDevice &other)
    : d(other.d)
{
}
BlockDevice &BlockDevice::operator=(const BlockDevice &rhs)
{
    if (this == &rhs)
        return *this;

    d = rhs.d;
    return *this;
}
BlockDevice::~BlockDevice()
{

}
void BlockDevice::setDeviceName(const QByteArray &deviceName)
{
    d->name = deviceName;
    readDeviceInfo();
}



void BlockDevice::readDeviceInfo()
{

    QFile file(PROC_PATH_DISK);
    if(!file.open(QIODevice::ReadOnly)) {
        return;
    }
    QList<QStringList> strList;
    QTextStream in(&file);
    QString line = in.readLine();
    strList << line.split(" ",QString::SkipEmptyParts);
    while (!line.isNull()) {
      line = in.readLine();
      strList << line.split(" ",QString::SkipEmptyParts);
    }
    file.close();

    for(int i = 0; i < strList.size(); ++i) {
        QStringList deviceInfo = strList[i];
        if( deviceInfo[2] == d->name ) {
            m_time_sec = QDateTime::currentSecsSinceEpoch();

            qint64 interval = m_time_sec - d->_time_Sec > 0 ? m_time_sec - d->_time_Sec :1;
            calcDiskIoStates(deviceInfo);
            readDeviceModel();
            readDeviceSize();
            if(d->read_iss != 0)
                d->r_ps = (deviceInfo[3].toULongLong() - d->read_iss) / static_cast<quint64>(interval);
            if(d->blk_read != 0)
                d->rsec_ps = (deviceInfo[5].toULongLong() - d->blk_read) / static_cast<quint64>(interval);
            if(d->read_merged != 0)
                d->rrqm_ps = (deviceInfo[4].toULongLong() - d->read_merged)/ static_cast<quint64>(interval);
            if(d->write_com !=0)
                d->w_ps = (deviceInfo[7].toULongLong() - d->write_com) / static_cast<quint64>(interval);
            if(d->write_merged != 0)
                d->wrqm_ps = (deviceInfo[8].toULongLong() - d->write_merged) / static_cast<quint64>(interval);
            d->blk_read = deviceInfo[5].toULongLong();
            d->bytes_read = deviceInfo[5].toULongLong() * SECTOR_SIZE;
            if(deviceInfo[3].toULongLong() != 0)
                d->p_rrqm =  deviceInfo[4].toDouble()/deviceInfo[3].toDouble() *100;
            d->tps = deviceInfo[3].toULongLong() + deviceInfo[7].toULongLong();
            d->blk_wrtn = deviceInfo[9].toULongLong();
            d->bytes_wrtn = deviceInfo[9].toULongLong() * SECTOR_SIZE;
            if(deviceInfo[7].toULongLong() != 0)
                d->p_wrqm = deviceInfo[8].toULongLong()/deviceInfo[7].toULongLong() * 100;
            d->read_iss = deviceInfo[3].toULongLong();
            d->write_com = deviceInfo[7].toULongLong();
            d->read_merged = deviceInfo[4].toULongLong();
            d->write_merged = deviceInfo[8].toULongLong();
            d->discard_sector = deviceInfo[16].toULongLong();
            d->_time_Sec = QDateTime::currentSecsSinceEpoch();

            //  d->blk_read =
            break;
        }
    }

}

void BlockDevice::readDeviceModel()
{
    QString Path = QString(SYSFS_PATH_MODEL).arg(d->name.data());
    QFile file(Path);
    if(!file.open(QIODevice::ReadOnly)) {
        return;
    }
    QString model = file.readLine().replace("\n","").trimmed();
    d->model = model;
    file.close();
}

void BlockDevice::readDeviceSize()
{
    QString path = QString(SYSFS_PATH_SIZE).arg(d->name.data());
    QFile file(path);
    if(!file.open(QIODevice::ReadOnly)) {
        return;
    }
    quint64 size = file.readLine().replace("\n","").trimmed().toULongLong() * SECTOR_SIZE;
    d->capacity = size;
    file.close();

}

void BlockDevice::calcDiskIoStates(const QStringList& diskInfo)
{
    quint64 curr_read_sector =  diskInfo[5].toULongLong();
    quint64 curr_write_sector = diskInfo[9].toULongLong();
    quint64 curr_discard_sector = diskInfo[16].toULongLong();

    // read increment between interval
    auto rdiff = (curr_read_sector > d->blk_read) ? (curr_read_sector - d->blk_read) : 0;
    // write increment between interval
    auto wdiff = (curr_write_sector > d->blk_wrtn) ? (curr_write_sector - d->blk_wrtn) : 0;
    // discarded increment between interval
    auto ddiff = (curr_discard_sector > d->discard_sector) ? (curr_discard_sector - d->discard_sector) : 0;
    // calculate actual size
    auto rsize = rdiff * SECTOR_SIZE;
    auto wsize = (wdiff + ddiff) * SECTOR_SIZE;
    auto interval = (m_time_sec > d->_time_Sec) ? (m_time_sec - d->_time_Sec) : 1;
    d->read_speed = rsize / static_cast<quint64>(interval);
    d->wirte_speed = wsize / static_cast<quint64>(interval);
}



} // namespace system
} // namespace core