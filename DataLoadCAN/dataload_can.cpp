#include <QTextStream>
#include <QFile>
#include <QMessageBox>
#include <QDebug>
#include <QSettings>
#include <QProgressDialog>
#include <QFileDialog>
#include <QRegularExpression>

#include <fstream>
#include <cstring>
#include <clocale>
#include "dataload_can.h"
#include "../PluginsCommonCAN/select_can_database.h"

// Regular expression for log files created by candump -L
// Captured groups: time, channel, frame_id, payload
const QRegularExpression canlog_rgx("\\((?<time>\\d*\\.\\d*)\\)\\s*(?<can_channel>[\\S]*)\\s*(?<id>[0-9a-fA-F]{3,8})\\#(?<data>[0-9a-fA-F]*)");
const QRegularExpression canfd_log_rgx("\\((?<time>\\d*\\.\\d*)\\)\\s*(?<can_channel>[\\S]*)\\s*(?<id>[0-9a-fA-F]{3,8})\\#(?<flag>\\#[0-1])(?<data>[0-9a-fA-F]*)");

DataLoadCAN::DataLoadCAN()
{
  extensions_.push_back("log");
}

const std::vector<const char*>& DataLoadCAN::compatibleFileExtensions() const
{
  return extensions_;
}

bool DataLoadCAN::loadCANDatabase(std::string dbc_file_location,
                                  CanFrameProcessor::CanProtocol protocol,
                                  PlotDataMapRef& plot_data_map,
                                  const std::unordered_map<std::string, QRegularExpression>& filter_list)
{
  std::ifstream dbc_file{ dbc_file_location };
  frame_processor_ = std::make_unique<CanFrameProcessor>(dbc_file,
                                                         protocol, 
                                                         plot_data_map,
                                                         filter_list);
  return true;
}

QSize DataLoadCAN::inspectFile(QFile* file)
{
  QTextStream inA(file);
  int linecount = 0;

  while (!inA.atEnd())
  {
    inA.readLine();
    linecount++;
  }

  QSize table_size;
  table_size.setWidth(4);
  table_size.setHeight(linecount);

  return table_size;
}

bool DataLoadCAN::readDataFromFile(FileLoadInfo* fileload_info, PlotDataMapRef& plot_data_map)
{
  bool use_provided_configuration = false;

  if (fileload_info->plugin_config.hasChildNodes())
  {
    use_provided_configuration = true;
    xmlLoadState(fileload_info->plugin_config.firstChildElement());
  }

  const int TIME_INDEX_NOT_DEFINED = -2;

  int time_index = TIME_INDEX_NOT_DEFINED;

  QFile file(fileload_info->filename);
  file.open(QFile::ReadOnly);

  std::vector<std::string> column_names;

  const QSize table_size = inspectFile(&file);
  const int tot_lines = table_size.height() - 1;
  const int columncount = table_size.width();
  file.close();

  DialogSelectCanDatabase* dialog = new DialogSelectCanDatabase();

  if (dialog->exec() != static_cast<int>(QDialog::Accepted))
  {
    return false;
  }
  // load dbc data file
  loadCANDatabase(dialog->GetDatabaseLocation().toStdString(),
                  dialog->GetCanProtocol(), 
                  plot_data_map,
                  dialog->getNameFilterList());

  file.open(QFile::ReadOnly);
  QTextStream inB(&file);

  bool interrupted = false;

  int linecount = 0;

  QProgressDialog progress_dialog;
  progress_dialog.setLabelText("Loading... please wait");
  progress_dialog.setWindowModality(Qt::ApplicationModal);
  progress_dialog.setRange(0, tot_lines);
  progress_dialog.setAutoClose(true);
  progress_dialog.setAutoReset(true);
  progress_dialog.show();

  bool monotonic_warning = false;
  // To have . as decimal seperator, save current locale and change it.
  const auto oldLocale = std::setlocale(LC_NUMERIC, nullptr);
  std::setlocale(LC_NUMERIC, "C");
  while (!inB.atEnd())
  {
    QString line = inB.readLine();
    // qDebug() << line;
    static QRegularExpressionMatchIterator rxIterator;
    if(!frame_processor_->isExtendedId())
    {
      rxIterator = canlog_rgx.globalMatch(line);
    } 
    else 
    {
      rxIterator = canfd_log_rgx.globalMatch(line);
    }
    if (!rxIterator.hasNext())
    {
      continue;  // skip invalid lines
    }
    QRegularExpressionMatch canFrame = rxIterator.next();
    uint64_t frameId = std::stoul(canFrame.captured("id").toStdString(), 0, 16);
    double frameTime = std::stod(canFrame.captured("time").toStdString());

    int dlc = canFrame.capturedLength("data") / 2;
    uint8_t frameDataBytes[MAX_DATA_SIZE];
    QByteArray buffer = QByteArray::fromHex(canFrame.captured("data").toUtf8());
    memcpy(frameDataBytes,buffer.data(), dlc);
    // apply id filter only when filter list is not empty
    if(dialog->getIdFilterList().find(frameId) == dialog->getIdFilterList().end() &&
       !dialog->getIdFilterList().empty())
    {
      continue;
    }
    frame_processor_->ProcessCanFrame(frameId, frameDataBytes, dlc, frameTime);
    //------ progress dialog --------------
    if (linecount++ % 100 == 0)
    {
      progress_dialog.setValue(linecount);
      QApplication::processEvents();

      if (progress_dialog.wasCanceled())
      {
        return false;
      }
    }
  }
  // Restore locale setting
  std::setlocale(LC_NUMERIC, oldLocale);
  file.close();

  if (interrupted)
  {
    progress_dialog.cancel();
    plot_data_map.numeric.clear();
  }

  if (monotonic_warning)
  {
    QString message =
        "Two consecutive samples had the same X value (i.e. time).\n"
        "Since PlotJuggler makes the assumption that timeseries are strictly monotonic, you "
        "might experience undefined behaviours.\n\n"
        "You have been warned...";
    QMessageBox::warning(0, tr("Warning"), message);
  }

  return true;
}

DataLoadCAN::~DataLoadCAN()
{
}

bool DataLoadCAN::xmlSaveState(QDomDocument& doc, QDomElement& parent_element) const
{
  QDomElement elem = doc.createElement("default");
  elem.setAttribute("time_axis", default_time_axis_.c_str());

  parent_element.appendChild(elem);
  return true;
}

bool DataLoadCAN::xmlLoadState(const QDomElement& parent_element)
{
  QDomElement elem = parent_element.firstChildElement("default");
  if (!elem.isNull())
  {
    if (elem.hasAttribute("time_axis"))
    {
      default_time_axis_ = elem.attribute("time_axis").toStdString();
      return true;
    }
  }
  return false;
}

uint64_t DataLoadCAN::getId (const uint64_t frame_id)
{
  return (EXTENDED_IDENTIFIER & frame_id)? (~EXTENDED_IDENTIFIER) & frame_id : frame_id;
}
