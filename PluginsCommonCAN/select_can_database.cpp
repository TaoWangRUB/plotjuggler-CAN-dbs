#include <QFileDialog>

#include "select_can_database.h"
#include "ui_select_can_database.h"

DialogSelectCanDatabase::DialogSelectCanDatabase(QWidget* parent)
  : QDialog(parent), 
    m_ui(new Ui::DialogSelectCanDatabase), 
    m_database_location{}, 
    m_protocol{}
{
  m_ui->setupUi(this);

  m_ui->protocolListBox->addItem(tr("RAW"), QVariant(true));
  m_ui->protocolListBox->addItem(tr("NMEA2K"), QVariant(true));
  m_ui->protocolListBox->addItem(tr("J1939"), QVariant(true));
  m_ui->protocolListBox->setCurrentIndex(0);
  m_ui->okButton->setEnabled(false);

  //m_ui->idFilterEdit->hide();
  //m_ui->nameFilterEdit->hide();

  connect(m_ui->okButton, &QPushButton::clicked, this, &DialogSelectCanDatabase::Ok);
  connect(m_ui->cancelButton, &QPushButton::clicked, this, &DialogSelectCanDatabase::Cancel);
  connect(m_ui->loadDatabaseButton, &QPushButton::clicked, this, &DialogSelectCanDatabase::ImportDatabaseLocation);
}
QString DialogSelectCanDatabase::GetDatabaseLocation() const
{
  return m_database_location;
}
CanFrameProcessor::CanProtocol DialogSelectCanDatabase::GetCanProtocol() const
{
  return m_protocol;
}

DialogSelectCanDatabase::~DialogSelectCanDatabase()
{
  delete m_ui;
}

void DialogSelectCanDatabase::Ok()
{
  updateConfig();
  accept();
}

void DialogSelectCanDatabase::Cancel()
{
  reject();
}

void DialogSelectCanDatabase::updateConfig()
{
  // update protocol
  auto m_protocoltext = m_ui->protocolListBox->currentText().toStdString();
  if (m_protocoltext == "RAW")
  {
    m_protocol = CanFrameProcessor::CanProtocol::RAW;
  }
  else if (m_protocoltext == "NMEA2K")
  {
    m_protocol = CanFrameProcessor::CanProtocol::NMEA2K;
  }
  else if (m_protocoltext == "J1939")
  {
    m_protocol = CanFrameProcessor::CanProtocol::J1939;
  }
  // update name filter
  if( !m_ui->nameFilterEdit->text().isEmpty())
  {
    // clear if filter list is not empty
    m_filter_list.clear();
    std::stringstream ss(m_ui->nameFilterEdit->text().toStdString());
    std::string token;
    std::vector<std::string> result;
    while(std::getline(ss, token, ',')) {
      token.erase(std::remove(token.begin(), token.end(), ' '), token.end());
      result.push_back(token);
    }
    std::for_each(result.begin(), result.end(),
                 [this](const auto& str){this->m_filter_list.emplace(str, QRegularExpression(str.c_str(), QRegularExpression::CaseInsensitiveOption));});
    for(const auto&[k, v] : m_filter_list)
    {
      qDebug() << k.c_str();
    }
  }
  // update id filter
  if( !m_ui->idFilterEdit->text().isEmpty())
  {
    // clear if filter list is not empty
    m_id_filter_list.clear();
    QRegularExpression re("(\\w+)");
    QString str = m_ui->idFilterEdit->text();
    auto it = re.globalMatch(str);
    while (it.hasNext()) 
    {
      m_id_filter_list.emplace(std::stoul(it.next().captured(1).toStdString(), 0, 16));
    }
    for(const auto& k : m_id_filter_list)
    {
      qDebug() << k;
    }
  }
}
void DialogSelectCanDatabase::ImportDatabaseLocation()
{
  m_database_location =
      QFileDialog::getOpenFileUrl(Q_NULLPTR, tr("Select CAN database"), QUrl(), tr("CAN database (*.dbc)"))
          .toLocalFile();
  // Since file is gotten, enable config box.
  m_ui->databaseConfigBox->setEnabled(true);
  m_ui->nameFilterEdit->setEnabled(true);
  // Since file is gotten, enable ok button.
  m_ui->okButton->setEnabled(true);
}
