#ifndef DIALOG_SELECT_CAN_DATABASE_H
#define DIALOG_SELECT_CAN_DATABASE_H

#include <QDialog>
#include <QString>
#include <QFile>
#include <QStringList>
#include <QCheckBox>
#include <QShortcut>
#include <QDomDocument>
#include "../PluginsCommonCAN/CanFrameProcessor.h"

QT_BEGIN_NAMESPACE
namespace Ui
{
class DialogSelectCanDatabase;
}
QT_END_NAMESPACE


class DialogSelectCanDatabase : public QDialog
{
  Q_OBJECT

public:
  explicit DialogSelectCanDatabase(QWidget* parent = nullptr);
  QString GetDatabaseLocation() const;
  CanFrameProcessor::CanProtocol GetCanProtocol() const;
  const std::unordered_map<std::string, QRegularExpression>& getNameFilterList() const {return m_filter_list;};
  const std::unordered_set<uint64_t>& getIdFilterList() const { return m_id_filter_list;};

  ~DialogSelectCanDatabase() override;

private slots:
  void Ok();
  void Cancel();

private:
  Ui::DialogSelectCanDatabase* m_ui;
  QString m_database_location;
  CanFrameProcessor::CanProtocol m_protocol;
  std::unordered_map<std::string, QRegularExpression> m_filter_list;
  std::unordered_set<uint64_t> m_id_filter_list;

  // update configuration
  void updateConfig();
  void ImportDatabaseLocation();
};

#endif  // DIALOG_SELECT_CAN_DATABASE_H
