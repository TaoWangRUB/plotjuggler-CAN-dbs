#pragma once

#include <QObject>
#include <QtPlugin>
#include <PlotJuggler/dataloader_base.h>
#include "../PluginsCommonCAN/CanFrameProcessor.h"

using namespace PJ;

const uint64_t EXTENDED_IDENTIFIER = 2147483648;
const uint8_t MAX_DATA_SIZE = 64;

class DataLoadCAN : public DataLoader
{
  Q_OBJECT
  Q_PLUGIN_METADATA(IID "facontidavide.PlotJuggler3.DataLoader")
  Q_INTERFACES(PJ::DataLoader)

public:
  DataLoadCAN();
  virtual const std::vector<const char *> &compatibleFileExtensions() const override;
  virtual QSize inspectFile(QFile *file);
  virtual bool readDataFromFile(FileLoadInfo *fileload_info, PlotDataMapRef &plot_data_map) override;
  bool loadCANDatabase(std::string dbc_file_location, 
                       CanFrameProcessor::CanProtocol protocol,
                       PlotDataMapRef &plot_data_map,
                       const std::unordered_map<std::string, QRegularExpression>& filter_list = {});
  virtual ~DataLoadCAN();

  virtual const char *name() const override
  {
    return "DataLoad CAN";
  }

  virtual bool xmlSaveState(QDomDocument &doc, QDomElement &parent_element) const override;
  virtual bool xmlLoadState(const QDomElement &parent_element) override;

private:
  uint64_t getId (const uint64_t frame_id);
private:
  std::vector<const char *> extensions_;
  std::string default_time_axis_;
  std::unique_ptr<CanFrameProcessor> frame_processor_;
  bool is_extended_id_ = false;
};
