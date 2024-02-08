#include <QTextStream>
#include <QFile>
#include <QFileDialog>
#include <QMessageBox>
#include <QDebug>
#include <QCanBus>
#include <QCanBusFrame>

#include <thread>
#include <mutex>
#include <chrono>
#include <thread>
#include <fstream>

#include "datastream_can.h"

/*
 * Controller Area Network Identifier structure
 *
 * bit 0-28	: CAN identifier (11/29 bit)
 * bit 29	: error message frame flag (0 = data frame, 1 = error message)
 * bit 30	: remote transmission request flag (1 = rtr frame)
 * bit 31	: frame format flag (0 = standard 11 bit, 1 = extended 29 bit)
 */

using namespace PJ;

DataStreamCAN::DataStreamCAN() : connect_dialog_{ new ConnectDialog() }
{
  connect(connect_dialog_, &QDialog::accepted, this, &DataStreamCAN::connectCanInterface);
}

void DataStreamCAN::connectCanInterface()
{
  const ConnectDialog::Settings p = connect_dialog_->settings();

  QString errorString;
  can_interface_ = QCanBus::instance()->createDevice(p.pluginName, 
                                                     p.deviceInterfaceName,
                                                     &errorString);
  if (!can_interface_)
  {
    qDebug() << tr("Error creating device '%1', interface '%2' reason: '%3'")
                    .arg(p.pluginName)
                    .arg(p.deviceInterfaceName)
                    .arg(errorString);
    return;
  }

  if (p.useConfigurationEnabled)
  {
    for (const ConnectDialog::ConfigurationItem& item : p.configurations)
      can_interface_->setConfigurationParameter(item.first, item.second);
  }

  if (!can_interface_->connectDevice())
  {
    qDebug() << tr("Connection error: %1").arg(can_interface_->errorString());

    delete can_interface_;
    can_interface_ = nullptr;
  }
  else
  {
    std::ifstream dbc_file{ p.canDatabaseLocation.toStdString() };
    frame_processor_ = std::make_unique<CanFrameProcessor>(dbc_file, 
                                                           p.protocol, 
                                                           dataMap(),
                                                           connect_dialog_->getFilterList());

    QVariant bitRate = can_interface_->configurationParameter(QCanBusDevice::BitRateKey);
    QString status = nullptr;
    if (bitRate.isValid())
    {
      const bool isCanFd =
                    can_interface_->configurationParameter(QCanBusDevice::CanFdKey).toBool();
      const QVariant dataBitRate =
                    can_interface_->configurationParameter(QCanBusDevice::DataBitRateKey);
      if (isCanFd && dataBitRate.isValid()) {
        status = tr("Plugin: %1, connected to %2 at %3 / %4 kBit/s")
                    .arg(p.pluginName).arg(p.deviceInterfaceName)
                    .arg(bitRate.toInt() / 1000).arg(dataBitRate.toInt() / 1000);
      } 
      else 
      {
        status = tr("Plugin: %1, connected to %2 at %3 kBit/s")
                   .arg(p.pluginName).arg(p.deviceInterfaceName)
                   .arg(bitRate.toInt() / 1000);
      }
    }
    else
    {
      status = tr("Backend: %1, connected to %2")
                      .arg(p.pluginName)
                      .arg(p.deviceInterfaceName);
    }
    qDebug() << status;
    
  }
}

bool DataStreamCAN::start(QStringList*)
{
  if (running_) {
    return running_;
  }
  connect_dialog_->show();
  int res = connect_dialog_->exec();
  if (res != QDialog::Accepted)
  {
    return false;
  }
  thread_ = std::thread([this]() { this->loop(); });
  return true;
}

void DataStreamCAN::shutdown()
{
  running_ = false;
  if (thread_.joinable())
    thread_.join();
}

bool DataStreamCAN::isRunning() const
{
  return running_;
}

DataStreamCAN::~DataStreamCAN()
{
  shutdown();
}

bool DataStreamCAN::xmlSaveState(QDomDocument& doc, QDomElement& parent_element) const
{
  return true;
}

bool DataStreamCAN::xmlLoadState(const QDomElement& parent_element)
{
  return true;
}

void DataStreamCAN::pushSingleCycle()
{
  std::lock_guard<std::mutex> lock(mutex());

  // Since readAllFrames is introduced in Qt5.12, reading using for
  auto n_frames = can_interface_->framesAvailable();
  for (int i = 0; i < n_frames; i++)
  {
    auto frame = can_interface_->readFrame();
    double timestamp = frame.timeStamp().seconds() + frame.timeStamp().microSeconds() * 1e-6;
    // apply id filter only when filter list is not empty
    if(connect_dialog_->getIdFilterList().find(frame.frameId()) == connect_dialog_->getIdFilterList().end() &&
       !connect_dialog_->getIdFilterList().empty())
    {
      continue;
    }
    frame_processor_->ProcessCanFrame(frame.frameId(), (const uint8_t*)frame.payload().data(), 8, timestamp);
  }
}

void DataStreamCAN::loop()
{
  // Block until both are initalized
  while (can_interface_ == nullptr || frame_processor_ == nullptr)
  {
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
  }
  running_ = true;
  while (running_)
  {
    auto prev = std::chrono::high_resolution_clock::now();
    pushSingleCycle();
    emit dataReceived();
    std::this_thread::sleep_until(prev + std::chrono::milliseconds(10));
  }
}

uint64_t DataStreamCAN::getId (const uint64_t frame_id)
{
  return (EXTENDED_IDENTIFIER & frame_id)? (~EXTENDED_IDENTIFIER) & frame_id : frame_id;
}