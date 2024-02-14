#include "CanFrameProcessor.h"
#include "N2kMsg/GenericFastPacket.h"

const uint64_t EXTENDED_IDENTIFIER = 0x80000000UL;
const uint8_t MAX_DATA_SIZE = 64;

CanFrameProcessor::CanFrameProcessor(std::ifstream& dbc_file, CanProtocol protocol, PJ::PlotDataMapRef& data_map)
  : protocol_{ protocol }, data_map_{ data_map }
{
  can_network_ = dbcppp::INetwork::LoadDBCFromIs(dbc_file);
  messages_.clear();
  for (const dbcppp::IMessage& msg : can_network_->Messages())
  {
    if (protocol_ == CanProtocol::RAW) {
      // When protocol is raw, use can_id from the dbc as the key for the messages
      if(!is_extended_id_)
      {
        is_extended_id_ = msg.Id() & EXTENDED_IDENTIFIER;
      }
      messages_.insert({getId(msg.Id()), &msg});
    }
    else {
      // When protocol is not raw, use PGN as the key for the messages_
      messages_.insert(std::make_pair(PGN_FROM_FRAME_ID(msg.Id()), &msg));
      // For N2kMsgFast, MessageSize is certainly larger than 8 bytes
      if (protocol_ == CanProtocol::NMEA2K)
      {
        if (msg.MessageSize() > 8)
        {
          fast_packet_pgns_set_.insert(PGN_FROM_FRAME_ID(msg.Id()));
        }
      }
    }
  }
}

bool CanFrameProcessor::ProcessCanFrame(const uint32_t frame_id, const uint8_t* payload_ptr, const size_t data_len,
                                        double timestamp_secs)
{
  if (can_network_)
  {
    switch (protocol_)
    {
      case CanProtocol::RAW:
      {
        return ProcessCanFrameRaw(frame_id, payload_ptr, data_len, timestamp_secs);
      }
      case CanProtocol::NMEA2K:
      {
        return ProcessCanFrameN2k(frame_id, payload_ptr, data_len, timestamp_secs);
      }
      case CanProtocol::J1939:
      {
        return ProcessCanFrameJ1939(frame_id, payload_ptr, data_len, timestamp_secs);
      }
      default:
        return false;
    }
  }
  else
  {
    return false;
  }
}

bool CanFrameProcessor::ProcessCanFrameRaw(const uint32_t frame_id, const uint8_t* data_ptr, const size_t data_len,
                                           const double timestamp_secs)
{
  auto msg_it = messages_.find(frame_id);
  if (msg_it != messages_.end())
  {
    const dbcppp::IMessage* msg = msg_it->second;
    for (const dbcppp::ISignal& sig : msg->Signals())
    {
      const dbcppp::ISignal* mux_sig = msg->MuxSignal();
      if (sig.MultiplexerIndicator() != dbcppp::ISignal::EMultiplexer::MuxValue ||
          (mux_sig && (mux_sig->Decode(data_ptr) == sig.MultiplexerSwitchValue())))
      {
        double decoded_val = sig.RawToPhys(sig.Decode(data_ptr));
        auto str = QString("can_frames/%1/%2")
                       .arg(QString::fromStdString(msg->Name()), QString::fromStdString(sig.Name()))
                       .toStdString();
        // qCritical() << str.c_str();
        auto it = data_map_.numeric.find(str);
        if (it != data_map_.numeric.end())
        {
          auto& plot = it->second;
          plot.pushBack({ timestamp_secs, decoded_val });
        }
        else
        {
          auto& plot = data_map_.addNumeric(str)->second;
          plot.pushBack({ timestamp_secs, decoded_val });
        }
      }
    }
    return true;
  }
  else
  {
    return false;
  }
}
bool CanFrameProcessor::ProcessCanFrameN2k(const uint32_t frame_id, const uint8_t* data_ptr, const size_t data_len,
                                           const double timestamp_secs)
{
  N2kMsgStandard n2k_msg(frame_id, data_ptr, data_len, timestamp_secs);

  if (fast_packet_pgns_set_.count(n2k_msg.GetPgn()))
  {
    fp_generic_fast_packet_t fp_unpacked;
    fp_generic_fast_packet_unpack(&fp_unpacked, n2k_msg.GetDataPtr(), FP_GENERIC_FAST_PACKET_LENGTH);

    // Find the current fast packet, return null one if it does not exists.
    auto current_fp_it = fast_packets_map_.find(n2k_msg.GetFrameId());
    auto& current_fp = current_fp_it != fast_packets_map_.end() ? current_fp_it->second : null_n2k_fast_ptr_;

    if (fp_unpacked.chunk_id == FP_GENERIC_FAST_PACKET_CHUNK_ID_FIRST_CHUNK_CHOICE)
    {
      // First chunk's data is only 6 bytes
      fast_packets_map_[n2k_msg.GetFrameId()] = std::make_unique<N2kMsgFast>(
          n2k_msg.GetFrameId(), n2k_msg.GetDataPtr() + 2, 6ul, timestamp_secs, fp_unpacked.len_bytes);
    }
    else
    {
      if (current_fp)
      {
        current_fp->AppendData(n2k_msg.GetDataPtr() + 1, 7ul);
      }
    }
    if (current_fp && current_fp->IsComplete())
    {
      ForwardN2kSignalsToPlot(*current_fp);
      current_fp.release();
      return true;
    }
  }
  else
  {
    ForwardN2kSignalsToPlot(n2k_msg);
    return true;
  }
  return false;
}
bool CanFrameProcessor::ProcessCanFrameJ1939(const uint32_t frame_id, const uint8_t* data_ptr, const size_t data_len,
                                             const double timestamp_secs)
{
  N2kMsgStandard n2k_msg(frame_id, data_ptr, data_len, timestamp_secs);
  ForwardN2kSignalsToPlot(n2k_msg);
  return true;
}

void CanFrameProcessor::ForwardN2kSignalsToPlot(const N2kMsgInterface& n2k_msg)
{
  auto protocol_prefix = protocol_ == CanProtocol::NMEA2K ? QString("nmea2k_msg") : QString("j1939_msg");
  // qCritical() << "frame_id:" << QString::number(dbc_id) << "\tcan_id:" << QString::number(n2k_msg.GetFrameId());
  auto messages_iter = messages_.find(n2k_msg.GetPgn());
  if (messages_iter != messages_.end())
  {
    const dbcppp::IMessage* msg = messages_iter->second;
    // qCritical() << "msg_name:" << QString::fromStdString(msg->Name());
    for (const dbcppp::ISignal& sig : msg->Signals())
    {
      const dbcppp::ISignal* mux_sig = msg->MuxSignal();
      if (sig.MultiplexerIndicator() != dbcppp::ISignal::EMultiplexer::MuxValue ||
          (mux_sig && (mux_sig->Decode(n2k_msg.GetDataPtr()) == sig.MultiplexerSwitchValue())))
      {
        double decoded_val = sig.RawToPhys(sig.Decode(n2k_msg.GetDataPtr()));
        std::string ts_name;
        if (n2k_msg.GetPduFormat() < 240)
        {
          auto destination_qstr = QString("%1").arg(n2k_msg.GetPduSpecific(), 2, 16, QLatin1Char('0')).toUpper();
          ts_name = QString("%1/PDUF1/%2 (0x%3)/0x%4/0x%5/%6")
                        .arg(protocol_prefix,
                             QString::fromStdString(msg->Name()),
                             QString("%1").arg(n2k_msg.GetPgn(), 4, 16, QLatin1Char('0')).toUpper(),
                             QString("%1").arg(n2k_msg.GetSourceAddr(), 2, 16, QLatin1Char('0')).toUpper(),
                             destination_qstr, QString::fromStdString(sig.Name()))
                        .toStdString();
        }
        else
        {
          ts_name = QString("%1/PDUF2/%2 (0x%3)/0x%4/%5")
                        .arg(protocol_prefix,
                             QString::fromStdString(msg->Name()),
                             QString("%1").arg(n2k_msg.GetPgn(), 5, 16, QLatin1Char('0')).toUpper(),
                             QString("%1").arg(n2k_msg.GetSourceAddr(), 2, 16, QLatin1Char('0')).toUpper(),
                             QString::fromStdString(sig.Name()))
                        .toStdString();
        }
        // qCritical() << str.c_str();
        auto it = data_map_.numeric.find(ts_name);
        if (it != data_map_.numeric.end())
        {
          auto& plot = it->second;
          plot.pushBack({ n2k_msg.GetTimeStamp(), decoded_val });
        }
        else
        {
          auto& plot = data_map_.addNumeric(ts_name)->second;
          plot.pushBack({ n2k_msg.GetTimeStamp(), decoded_val });
        }
      }
    }
  }
}

uint64_t CanFrameProcessor::getId (const uint64_t frame_id)
{
  return (EXTENDED_IDENTIFIER & frame_id)? (~EXTENDED_IDENTIFIER) & frame_id : frame_id;
}