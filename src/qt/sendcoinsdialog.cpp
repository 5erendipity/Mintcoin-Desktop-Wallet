#include "sendcoinsdialog.h"
#include "ui_sendcoinsdialog.h"
#include "init.h"
#include "walletmodel.h"
#include "bitcoinunits.h"
#include "addresstablemodel.h"
#include "addressbookpage.h"
#include "bitcoinunits.h"
#include "addressbookpage.h"
#include "optionsmodel.h"
#include "sendcoinsentry.h"
#include "guiutil.h"
#include "askpassphrasedialog.h"
#include "coincontrol.h"
#include "coincontroldialog.h"
#include "bitcoingui.h"
#include "guiconstants.h"
#include "walletview.h"

#include <QMessageBox>
#include <QTextDocument>
#include <QScrollBar>
#include <QClipboard>

SendCoinsDialog::SendCoinsDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::SendCoinsDialog),
    model(0)
{
    ui->setupUi(this);
    walletView=(WalletView*) parent;

#ifdef Q_OS_MAC // Icons on push buttons are very uncommon on Mac
    ui->addButton->setIcon(QIcon());
    ui->clearButton->setIcon(QIcon());
    ui->sendButton->setIcon(QIcon());
#endif

#if QT_VERSION >= 0x040700
     /* Do not move this to the XML file, Qt before 4.7 will choke on it */
     ui->lineEditCoinControlChange->setPlaceholderText(tr("Enter a Mintcoin address (e.g. MrXW1RKLDe8VMNwTwLwSiKuATN5M74EL85)"));
#endif

    addEntry();

    connect(ui->addButton, SIGNAL(clicked()), this, SLOT(addEntry()));
    connect(ui->clearButton, SIGNAL(clicked()), this, SLOT(clear()));
    connect(ui->sendRecurring, SIGNAL(clicked()), this, SLOT(addRecurring()));
	
	    // Coin Control
     ui->lineEditCoinControlChange->setFont(GUIUtil::bitcoinAddressFont());
     connect(ui->pushButtonCoinControl, SIGNAL(clicked()), this, SLOT(coinControlButtonClicked()));
     connect(ui->checkBoxCoinControlChange, SIGNAL(stateChanged(int)), this, SLOT(coinControlChangeChecked(int)));
     connect(ui->lineEditCoinControlChange, SIGNAL(textEdited(const QString &)), this, SLOT(coinControlChangeEdited(const QString &)));
 
		// Coin Control: clipboard actions
     QAction *clipboardQuantityAction = new QAction(tr("Copy quantity"), this);
     QAction *clipboardAmountAction = new QAction(tr("Copy amount"), this);
     QAction *clipboardFeeAction = new QAction(tr("Copy fee"), this);
     QAction *clipboardAfterFeeAction = new QAction(tr("Copy after fee"), this);
     QAction *clipboardBytesAction = new QAction(tr("Copy bytes"), this);
     QAction *clipboardPriorityAction = new QAction(tr("Copy priority"), this);
     QAction *clipboardLowOutputAction = new QAction(tr("Copy low output"), this);
     QAction *clipboardChangeAction = new QAction(tr("Copy change"), this);
     connect(clipboardQuantityAction, SIGNAL(triggered()), this, SLOT(coinControlClipboardQuantity()));
     connect(clipboardAmountAction, SIGNAL(triggered()), this, SLOT(coinControlClipboardAmount()));
     connect(clipboardFeeAction, SIGNAL(triggered()), this, SLOT(coinControlClipboardFee()));
     connect(clipboardAfterFeeAction, SIGNAL(triggered()), this, SLOT(coinControlClipboardAfterFee()));
     connect(clipboardBytesAction, SIGNAL(triggered()), this, SLOT(coinControlClipboardBytes()));
     connect(clipboardPriorityAction, SIGNAL(triggered()), this, SLOT(coinControlClipboardPriority()));
     connect(clipboardLowOutputAction, SIGNAL(triggered()), this, SLOT(coinControlClipboardLowOutput()));
     connect(clipboardChangeAction, SIGNAL(triggered()), this, SLOT(coinControlClipboardChange()));
     ui->labelCoinControlQuantity->addAction(clipboardQuantityAction);
     ui->labelCoinControlAmount->addAction(clipboardAmountAction);
     ui->labelCoinControlFee->addAction(clipboardFeeAction);
     ui->labelCoinControlAfterFee->addAction(clipboardAfterFeeAction);
     ui->labelCoinControlBytes->addAction(clipboardBytesAction);
     ui->labelCoinControlPriority->addAction(clipboardPriorityAction);
     ui->labelCoinControlLowOutput->addAction(clipboardLowOutputAction);
     ui->labelCoinControlChange->addAction(clipboardChangeAction);
 
    fNewRecipientAllowed = true;

}

void SendCoinsDialog::setModel(WalletModel *model)
{
    this->model = model;

    if(model && model->getOptionsModel())
    {
        for(int i = 0; i < ui->entries->count(); ++i)
        {
            SendCoinsEntry *entry = qobject_cast<SendCoinsEntry*>(ui->entries->itemAt(i)->widget());
            if(entry)
            {
                entry->setModel(model);
            }
        }

        setBalance(model->getBalance(), model->getStake(), model->getUnconfirmedBalance(), model->getImmatureBalance(), model->getMintedBalance());
        connect(model, SIGNAL(balanceChanged(qint64, qint64, qint64, qint64, qint64)), this, SLOT(setBalance(qint64, qint64, qint64, qint64, qint64)));
        connect(model->getOptionsModel(), SIGNAL(displayUnitChanged(int)), this, SLOT(updateDisplayUnit()));
		
        // Coin Control
        connect(model->getOptionsModel(), SIGNAL(displayUnitChanged(int)), this, SLOT(coinControlUpdateLabels()));
        connect(model->getOptionsModel(), SIGNAL(coinControlFeaturesChanged(bool)), this, SLOT(coinControlFeatureChanged(bool)));
        connect(model->getOptionsModel(), SIGNAL(transactionFeeChanged(qint64)), this, SLOT(coinControlUpdateLabels()));
        ui->frameCoinControl->setVisible(model->getOptionsModel()->getCoinControlFeatures());
        ui->payFromFrame->setVisible(!model->getOptionsModel()->getCoinControlFeatures());
        ui->sendRecurring->setEnabled(!model->getOptionsModel()->getCoinControlFeatures());
        ui->recurringDays->setEnabled(!model->getOptionsModel()->getCoinControlFeatures());
        coinControlUpdateLabels();
    }

}

SendCoinsDialog::~SendCoinsDialog()
{
    delete ui;
}

void SendCoinsDialog::on_sendButton_clicked()
{
    if(!model || !model->getOptionsModel())
        return;

    QList<SendCoinsRecipient> recipients;

    bool valid = true;

    if(model->getEncryptionStatus() != WalletModel::Unencrypted && model->getOptionsModel()->getPasswordOnSend())
    {
        AskPassphraseDialog dlg(AskPassphraseDialog::PassToSend,this);
        dlg.setModel(model);
        if(!dlg.exec())
          return;
    }

    for(int i = 0; i < ui->entries->count(); ++i)
    {
        SendCoinsEntry *entry = qobject_cast<SendCoinsEntry*>(ui->entries->itemAt(i)->widget());
        if(entry)
        {
            if(entry->validate())
            {
                recipients.append(entry->getValue());
            }
            else
            {
                valid = false;
            }
        }
    }

    if(!valid || recipients.isEmpty())
    {
        return;
    }

    // Format confirmation message
    QStringList formatted;
    foreach(const SendCoinsRecipient &rcp, recipients)
    {
        QString amount = BitcoinUnits::formatWithUnit(model->getOptionsModel()->getDisplayUnit(), rcp.amount);
        if (rcp.authenticatedMerchant.isEmpty())
        {
            QString address = rcp.address;
            QString to = GUIUtil::HtmlEscape(rcp.label);
            formatted.append(tr("<b>%1</b> to %2 (%3)").arg(amount, to, address));
        }
        else
        {
            QString merchant = GUIUtil::HtmlEscape(rcp.authenticatedMerchant);
            formatted.append(tr("<b>%1</b> to %2").arg(amount, merchant));
        }
    }

    fNewRecipientAllowed = false;

    QMessageBox::StandardButton retval = QMessageBox::question(this, tr("Confirm send coins"),
                          tr("Are you sure you want to send %1?").arg(formatted.join(tr(" and "))),
          QMessageBox::Yes|QMessageBox::Cancel,
          QMessageBox::Cancel);

    if(retval != QMessageBox::Yes)
    {
        fNewRecipientAllowed = true;
        return;
    }

    WalletModel::UnlockContext ctx(model->requestUnlock());
    if(!ctx.isValid())
    {
        // Unlock wallet was cancelled
        fNewRecipientAllowed = true;
        return;
    }

       WalletModel::SendCoinsReturn sendstatus;

    if (!model->getOptionsModel() || !model->getOptionsModel()->getCoinControlFeatures())
    {
      if(ui->payFrom->itemText(ui->payFrom->currentIndex())=="Any Address")
      {
        sendstatus = model->sendCoins(recipients);
      }
      else
      {
        CCoinControl coinControlByAddress;
        coinControlByAddress.setUseOnlyMinCoinAge();
        std::map<QString, std::vector<COutput> > mapCoins;
        model->listCoins(mapCoins);
        BOOST_FOREACH(PAIRTYPE(QString, std::vector<COutput>) coins, mapCoins)
        {
            QString sWalletAddress = coins.first;
            if(ui->payFrom->itemData(ui->payFrom->currentIndex()).toString() == sWalletAddress)
            {
                BOOST_FOREACH(const COutput& out, coins.second)
                {
                  COutPoint outpt(out.tx->GetHash(), out.i);
                  coinControlByAddress.Select(outpt);
                }
            }
        }
        sendstatus = model->sendCoins(recipients, &coinControlByAddress);
      }
    }
    else
        sendstatus = model->sendCoins(recipients, CoinControlDialog::coinControl);

    switch(sendstatus.status)
    {
    case WalletModel::InvalidAddress:
        QMessageBox::warning(this, tr("Send Coins"),
            tr("The recipient address is not valid, please recheck."),
            QMessageBox::Ok, QMessageBox::Ok);
        break;
    case WalletModel::InvalidAmount:
        QMessageBox::warning(this, tr("Send Coins"),
            tr("The amount to pay must be larger than 0."),
            QMessageBox::Ok, QMessageBox::Ok);
        break;
    case WalletModel::AmountExceedsBalance:
        QMessageBox::warning(this, tr("Send Coins"),
            tr("The amount exceeds your balance."),
            QMessageBox::Ok, QMessageBox::Ok);
        break;
    case WalletModel::AmountWithFeeExceedsBalance:
        QMessageBox::warning(this, tr("Send Coins"),
            tr("The total exceeds your balance when the %1 transaction fee is included.").
            arg(BitcoinUnits::formatWithUnit(model->getOptionsModel()->getDisplayUnit(), sendstatus.fee)),
            QMessageBox::Ok, QMessageBox::Ok);
        break;
    case WalletModel::DuplicateAddress:
        QMessageBox::warning(this, tr("Send Coins"),
            tr("Duplicate address found, can only send to each address once per send operation."),
            QMessageBox::Ok, QMessageBox::Ok);
        break;
    case WalletModel::TransactionCreationFailed:
        QMessageBox::warning(this, tr("Send Coins"),
            tr("Error: Transaction creation failed."),
            QMessageBox::Ok, QMessageBox::Ok);
        break;
    case WalletModel::TransactionCommitFailed:
        QMessageBox::warning(this, tr("Send Coins"),
            tr("Error: The transaction was rejected. This might happen if some of the coins in your wallet were already spent, such as if you used a copy of wallet.dat and coins were spent in the copy but not marked as spent here."),
            QMessageBox::Ok, QMessageBox::Ok);
        break;
    case WalletModel::Aborted: // User aborted, nothing to do
        break;
    case WalletModel::OK:
        accept();
		CoinControlDialog::coinControl->UnSelectAll();
        coinControlUpdateLabels();
        break;
    }
    fNewRecipientAllowed = true;
}

void SendCoinsDialog::addRecurring()
{
  QString from=ui->payFrom->itemData(ui->payFrom->currentIndex()).toString();
  int repeatDays= ui->recurringDays->text().split(" ")[0].toInt();
  if(repeatDays==0)
  {
    ui->recurringDays->setStyleSheet(STYLE_INVALID);
    return;
  }
  ui->recurringDays->setStyleSheet("");

  if(from!=QString("Any Address"))
  {
    if(!model->validateAddress(from))
    {
      ui->payFrom->setStyleSheet(STYLE_INVALID);
      return;
    }
  }
  ui->payFrom->setStyleSheet("");
  bool valid = true;
  for(int i = 0; i < ui->entries->count(); ++i)
  {
      SendCoinsEntry *entry = qobject_cast<SendCoinsEntry*>(ui->entries->itemAt(i)->widget());
      if(entry)
      {
          if(!entry->validate())
          {
              valid = false;
          }
      }
  }
  if(valid)
  {
      walletView->recurringSendPage->addRecurringEntry(from , repeatDays);
      for(int i = 0; i < ui->entries->count(); ++i)
      {
          SendCoinsEntry *entry = qobject_cast<SendCoinsEntry*>(ui->entries->itemAt(i)->widget());
          walletView->recurringSendPage->addRecurringRecipient(entry->getValue());
      }
      accept();
  }
}


void SendCoinsDialog::clear()
{
    // Remove entries until only one left
    while(ui->entries->count())
    {
        delete ui->entries->takeAt(0)->widget();
    }
    addEntry();

    updateRemoveEnabled();

    ui->sendButton->setDefault(true);
}

void SendCoinsDialog::reject()
{
    clear();
}

void SendCoinsDialog::accept()
{
    clear();
}

SendCoinsEntry *SendCoinsDialog::addEntry()
{
    SendCoinsEntry *entry = new SendCoinsEntry(this);
    entry->setModel(model);
    ui->entries->addWidget(entry);
    connect(entry, SIGNAL(removeEntry(SendCoinsEntry*)), this, SLOT(removeEntry(SendCoinsEntry*)));
	connect(entry, SIGNAL(payAmountChanged()), this, SLOT(coinControlUpdateLabels()));

    updateRemoveEnabled();

    // Focus the field, so that entry can start immediately
    entry->clear();
    entry->setFocus();
    ui->scrollAreaWidgetContents->resize(ui->scrollAreaWidgetContents->sizeHint());
    qApp->processEvents();
    QScrollBar* bar = ui->scrollArea->verticalScrollBar();
    if(bar)
        bar->setSliderPosition(bar->maximum());
    return entry;
}

void SendCoinsDialog::updateRemoveEnabled()
{
    // Remove buttons are enabled as soon as there is more than one send-entry
    bool enabled = (ui->entries->count() > 1);
    for(int i = 0; i < ui->entries->count(); ++i)
    {
        SendCoinsEntry *entry = qobject_cast<SendCoinsEntry*>(ui->entries->itemAt(i)->widget());
        if(entry)
        {
            entry->setRemoveEnabled(enabled);
        }
    }
    setupTabChain(0);
	coinControlUpdateLabels();
}

void SendCoinsDialog::removeEntry(SendCoinsEntry* entry)
{
    delete entry;
    updateRemoveEnabled();
}

QWidget *SendCoinsDialog::setupTabChain(QWidget *prev)
{
    for(int i = 0; i < ui->entries->count(); ++i)
    {
        SendCoinsEntry *entry = qobject_cast<SendCoinsEntry*>(ui->entries->itemAt(i)->widget());
        if(entry)
        {
            prev = entry->setupTabChain(prev);
        }
    }
    QWidget::setTabOrder(prev, ui->addButton);
    QWidget::setTabOrder(ui->addButton, ui->sendButton);
    return ui->sendButton;
}

void SendCoinsDialog::setAddress(const QString &address)
{
    SendCoinsEntry *entry = 0;
    // Replace the first entry if it is still unused
    if(ui->entries->count() == 1)
    {
        SendCoinsEntry *first = qobject_cast<SendCoinsEntry*>(ui->entries->itemAt(0)->widget());
        if(first->isClear())
        {
            entry = first;
        }
    }
    if(!entry)
    {
        entry = addEntry();
    }

    entry->setAddress(address);
}

void SendCoinsDialog::pasteEntry(const SendCoinsRecipient &rv)
{
    if(!fNewRecipientAllowed)
        return;

    SendCoinsEntry *entry = 0;
    // Replace the first entry if it is still unused
    if(ui->entries->count() == 1)
    {
        SendCoinsEntry *first = qobject_cast<SendCoinsEntry*>(ui->entries->itemAt(0)->widget());
        if(first->isClear())
        {
            entry = first;
        }
    }
    if(!entry)
    {
        entry = addEntry();
    }

    entry->setValue(rv);
}

bool SendCoinsDialog::handlePaymentRequest(const SendCoinsRecipient &rv)
{
    if (!rv.authenticatedMerchant.isEmpty()) {
        // Expired payment request?
        const payments::PaymentDetails& details = rv.paymentRequest.getDetails();
        if (details.has_expires() && (int64)details.expires() < GetTime())
        {
            QMessageBox::warning(this, tr("Send Coins"),
                                 tr("Payment request expired"));
            return false;
        }
    }
    else {
        CBitcoinAddress address(rv.address.toStdString());
        if (!address.IsValid()) {
            QString strAddress(address.ToString().c_str());
            QMessageBox::warning(this, tr("Send Coins"),
                                 tr("Invalid payment address %1").arg(strAddress));
            return false;
        }
    }

    pasteEntry(rv);
    return true;
}

void SendCoinsDialog::updateBalance(qint64 balance)
{
  if(model && model->getOptionsModel())
  {
    // Update labelBalance with the current balance and the current unit
    ui->labelBalance->setText(BitcoinUnits::formatWithUnit(model->getOptionsModel()->getDisplayUnit(), balance));

    // Update the pay from combo box
    int currentIndex = ui->payFrom->currentIndex();
    ui->payFrom->clear();
    ui->payFrom->addItem("Any Address","Any Address");
    AddressTableModel* addressTableModel = model->getAddressTableModel();
    int numRows = 0;
    if(addressTableModel)
    {
      numRows = addressTableModel->rowCount(QModelIndex());
    }
    for(int j=0; j<numRows; ++j)
    {
      QVariant type = addressTableModel->index(j, 0, QModelIndex()).data(AddressTableModel::TypeRole);
      if(type==AddressTableModel::Receive)
      {
        QVariant address = addressTableModel->index(j, AddressTableModel::Address, QModelIndex()).data(Qt::DisplayRole);
        QVariant label = addressTableModel->index(j, AddressTableModel::Label, QModelIndex()).data(Qt::DisplayRole);
        QVariant amount = addressTableModel->index(j, AddressTableModel::Amount, QModelIndex()).data(Qt::DisplayRole);
        QString amountTextUnits = BitcoinUnits::name(model->getOptionsModel()->getDisplayUnit());
        ui->payFrom->addItem(label.toString() + "   " + address.toString() + "  " +
                             amount.toString() + " " + amountTextUnits, address.toString());
      }
    }
    ui->payFrom->setCurrentIndex(currentIndex);
  }
}


void SendCoinsDialog::setBalance(qint64 balance, qint64 stake, qint64 unconfirmedBalance, qint64 immatureBalance, qint64 mintedBalance)
{
    Q_UNUSED(stake);
    Q_UNUSED(unconfirmedBalance);
    Q_UNUSED(immatureBalance);
    Q_UNUSED(mintedBalance);

    updateBalance(balance);
}

void SendCoinsDialog::updateDisplayUnit()
{
    updateBalance(model->getBalance());
}

 // Coin Control: copy label "Quantity" to clipboard
 void SendCoinsDialog::coinControlClipboardQuantity()
 {
     QApplication::clipboard()->setText(ui->labelCoinControlQuantity->text());
 }
 
 // Coin Control: copy label "Amount" to clipboard
 void SendCoinsDialog::coinControlClipboardAmount()
 {
     QApplication::clipboard()->setText(ui->labelCoinControlAmount->text().left(ui->labelCoinControlAmount->text().indexOf(" ")));
 }
 
 // Coin Control: copy label "Fee" to clipboard
 void SendCoinsDialog::coinControlClipboardFee()
 {
     QApplication::clipboard()->setText(ui->labelCoinControlFee->text().left(ui->labelCoinControlFee->text().indexOf(" ")));
 }
 
 // Coin Control: copy label "After fee" to clipboard
 void SendCoinsDialog::coinControlClipboardAfterFee()
 {
     QApplication::clipboard()->setText(ui->labelCoinControlAfterFee->text().left(ui->labelCoinControlAfterFee->text().indexOf(" ")));
 }
 
 // Coin Control: copy label "Bytes" to clipboard
 void SendCoinsDialog::coinControlClipboardBytes()
 {
     QApplication::clipboard()->setText(ui->labelCoinControlBytes->text());
 }
 
 // Coin Control: copy label "Priority" to clipboard
 void SendCoinsDialog::coinControlClipboardPriority()
 {
     QApplication::clipboard()->setText(ui->labelCoinControlPriority->text());
 }
 
 // Coin Control: copy label "Low output" to clipboard
 void SendCoinsDialog::coinControlClipboardLowOutput()
 {
     QApplication::clipboard()->setText(ui->labelCoinControlLowOutput->text());
 }
 
 // Coin Control: copy label "Change" to clipboard
 void SendCoinsDialog::coinControlClipboardChange()
 {
     QApplication::clipboard()->setText(ui->labelCoinControlChange->text().left(ui->labelCoinControlChange->text().indexOf(" ")));
 }
 
 // Coin Control: settings menu - coin control enabled/disabled by user
 void SendCoinsDialog::coinControlFeatureChanged(bool checked)
 {
     ui->frameCoinControl->setVisible(checked);
     ui->payFromFrame->setVisible(!checked);
     ui->sendRecurring->setEnabled(!checked);
     ui->recurringDays->setEnabled(!checked);
 
     if (!checked && model) // coin control features disabled
         CoinControlDialog::coinControl->SetNull();
 }
 
 // Coin Control: button inputs -> show actual coin control dialog
 void SendCoinsDialog::coinControlButtonClicked()
 {
     CoinControlDialog dlg;
     dlg.setModel(model);
     dlg.exec();
     coinControlUpdateLabels();
 }
 
 // Coin Control: checkbox custom change address
 void SendCoinsDialog::coinControlChangeChecked(int state)
 {
     if (model)
     {
         if (state == Qt::Checked)
             CoinControlDialog::coinControl->destChange = CBitcoinAddress(ui->lineEditCoinControlChange->text().toStdString()).Get();
         else
             CoinControlDialog::coinControl->destChange = CNoDestination();
     }
 
     ui->lineEditCoinControlChange->setEnabled((state == Qt::Checked));
     ui->labelCoinControlChangeLabel->setEnabled((state == Qt::Checked));
 }
 
 // Coin Control: custom change address changed
 void SendCoinsDialog::coinControlChangeEdited(const QString & text)
 {
     if (model)
     {
         CoinControlDialog::coinControl->destChange = CBitcoinAddress(text.toStdString()).Get();
 
         // label for the change address
         ui->labelCoinControlChangeLabel->setStyleSheet("QLabel{color:black;}");
         if (text.isEmpty())
             ui->labelCoinControlChangeLabel->setText("");
         else if (!CBitcoinAddress(text.toStdString()).IsValid())
         {
             ui->labelCoinControlChangeLabel->setStyleSheet("QLabel{color:red;}");
             ui->labelCoinControlChangeLabel->setText(tr("WARNING: Invalid Bitcoin address"));
         }
         else
         {
             QString associatedLabel = model->getAddressTableModel()->labelForAddress(text);
             if (!associatedLabel.isEmpty())
                 ui->labelCoinControlChangeLabel->setText(associatedLabel);
             else
             {
                 CPubKey pubkey;
                 CKeyID keyid;
                 CBitcoinAddress(text.toStdString()).GetKeyID(keyid);   
                 if (model->getPubKey(keyid, pubkey))
                     ui->labelCoinControlChangeLabel->setText(tr("(no label)"));
                 else
                 {
                     ui->labelCoinControlChangeLabel->setStyleSheet("QLabel{color:red;}");
                     ui->labelCoinControlChangeLabel->setText(tr("WARNING: unknown change address"));
                 }
             }
         }
     }
 }
 
 // Coin Control: update labels
 void SendCoinsDialog::coinControlUpdateLabels()
 {
     if (!model || !model->getOptionsModel() || !model->getOptionsModel()->getCoinControlFeatures())
         return;
     // set pay amounts
    CoinControlDialog::payAmounts.clear();
    for(int i = 0; i < ui->entries->count(); ++i)
    {
        SendCoinsEntry *entry = qobject_cast<SendCoinsEntry*>(ui->entries->itemAt(i)->widget());
        if(entry)
            CoinControlDialog::payAmounts.append(entry->getValue().amount);
    }
     if (CoinControlDialog::coinControl->HasSelected())
    {
        // actual coin control calculation
         CoinControlDialog::updateLabels(model, this);
        // show coin control stats
        ui->labelCoinControlAutomaticallySelected->hide();
        ui->widgetCoinControl->show();
    }
    else
    {
        // hide coin control stats
        ui->labelCoinControlAutomaticallySelected->show();
        ui->widgetCoinControl->hide();
        ui->labelCoinControlInsuffFunds->hide();
    }
}	

	
