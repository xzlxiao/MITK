/*===================================================================

The Medical Imaging Interaction Toolkit (MITK)

Copyright (c) German Cancer Research Center,
Division of Medical and Biological Informatics.
All rights reserved.

This software is distributed WITHOUT ANY WARRANTY; without
even the implied warranty of MERCHANTABILITY or FITNESS FOR
A PARTICULAR PURPOSE.

See LICENSE.txt or http://www.mitk.org for details.

===================================================================*/

#include "QmitkCTKPythonShell.h"

#include "mitkNodePredicateDataType.h"

#include "QmitkDataStorageComboBox.h"
#include "QmitkStdMultiWidget.h"

#include "mitkDataStorageEditorInput.h"

// berry Includes
#include <berryPlatform.h>
#include <berryIWorkbenchPage.h>
#include <berryConstants.h>

#include <QMessageBox>
#include <QStringList>
#include <QList>
#include <QUrl>

#include <Python.h>


QmitkCTKPythonShell::QmitkCTKPythonShell(ctkAbstractPythonManager* pythonManager, QWidget* _parent)
{
  this->initialize( pythonManager );
  m_PythonManager = pythonManager;

  QmitkPythonMediator::getInstance()->registerPasteCommandClient( this );
}

QmitkCTKPythonShell::~QmitkCTKPythonShell()
{
  QmitkPythonMediator::getInstance()->unregisterPasteCommandClient( this );
}

void QmitkCTKPythonShell::dragEnterEvent(QDragEnterEvent *event)
{
  event->accept();
}
void QmitkCTKPythonShell::paste(const QString& command)
{
  m_PythonManager->executeString( command );
}

void QmitkCTKPythonShell::dropEvent(QDropEvent *event)
{
  QList<QUrl> urls = event->mimeData()->urls();
  for(int i = 0; i < urls.size(); i++)
  {
    m_PythonManager->executeString(urls[i].toString());
  }
}

bool QmitkCTKPythonShell::canInsertFromMimeData( const QMimeData *source ) const
{
  return true;
}

void QmitkCTKPythonShell::executeCommand(const QString& command)
{
  emit this->executeCommandSignal(command);
  ctkPythonConsole::executeCommand(command);
  QmitkPythonMediator::getInstance()->update();
}
