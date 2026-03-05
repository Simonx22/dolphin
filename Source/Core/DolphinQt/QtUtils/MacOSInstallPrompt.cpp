// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/QtUtils/MacOSInstallPrompt.h"

#include <QMessageBox>
#include <QProcess>
#include <QPushButton>

#include <cstdlib>
#include <vector>

#include "DolphinQt/QtUtils/ModalMessageBox.h"
#include "DolphinQt/Resources.h"

#ifdef __APPLE__
#include <sys/mount.h>

#include "Common/FileUtil.h"
#include "Common/StringUtil.h"
#endif

namespace QtUtils
{
#ifdef __APPLE__
namespace
{
constexpr std::string_view GLOBAL_APPLICATIONS_DIR = "/Applications";

bool IsBundleOnReadOnlyDiskImage(const std::string& bundle_path)
{
  if (!bundle_path.starts_with("/Volumes/"))
    return false;

  struct statfs mount_info = {};
  if (statfs(bundle_path.c_str(), &mount_info) != 0)
    return true;

  return (mount_info.f_flags & MNT_RDONLY) != 0;
}

std::vector<std::string> GetInstallTargets(const std::string& source_bundle_path)
{
  const std::string bundle_name = PathToFileName(source_bundle_path);

  std::vector<std::string> install_dirs;
  install_dirs.emplace_back(GLOBAL_APPLICATIONS_DIR);

  if (const char* home = getenv("HOME"); home && home[0] != '\0')
  {
    const std::string user_apps_dir = std::string(home) + "/Applications";
    if (user_apps_dir != GLOBAL_APPLICATIONS_DIR)
      install_dirs.emplace_back(user_apps_dir);
  }

  std::vector<std::string> targets;
  for (const std::string& dir_path : install_dirs)
    targets.emplace_back(dir_path + "/" + bundle_name);

  return targets;
}

bool InstallBundle(const std::string& source_bundle_path, const std::string& destination_path,
                   QString* error)
{
  if (StringToPath(source_bundle_path).lexically_normal() ==
      StringToPath(destination_path).lexically_normal())
  {
    return true;
  }

  const std::string destination_dir = PathToString(StringToPath(destination_path).parent_path());
  if (!File::IsDirectory(destination_dir) && !File::CreateDirs(destination_dir))
  {
    *error = QObject::tr("Unable to create destination folder: %1")
                 .arg(QString::fromStdString(destination_dir));
    return false;
  }

  if (File::Exists(destination_path) && !File::IsDirectory(destination_path))
  {
    *error = QObject::tr("The destination path exists and is not an app bundle folder.");
    return false;
  }

  if (File::Copy(source_bundle_path, destination_path, true))
    return true;

  *error = QObject::tr("Couldn't copy app bundle to destination.");
  return false;
}

bool RelaunchInstalledApp(const std::string& destination_path)
{
  return QProcess::startDetached(QStringLiteral("/usr/bin/open"),
                                 {QStringLiteral("-n"), QString::fromStdString(destination_path)});
}
}  // namespace
#endif

bool MaybeInstallFromDiskImage()
{
#ifndef __APPLE__
  return true;
#else
  const std::string bundle_path = File::GetBundleDirectory();
  if (!IsBundleOnReadOnlyDiskImage(bundle_path))
    return true;

  const QString app_name = QObject::tr("Dolphin Emulator");

  ModalMessageBox prompt(nullptr, Qt::ApplicationModal);
  prompt.setWindowTitle(QObject::tr("Install %1").arg(app_name));
  prompt.setIconPixmap(Resources::GetAppIcon().pixmap(72, 72));
  prompt.setText(QObject::tr("Do you want to install %1?").arg(app_name));
  prompt.setInformativeText(
      QObject::tr("You're running %1 from its disk image. Installing it on your Mac lets "
                  "you run it without the disk image, and makes it easier to install updates.")
          .arg(app_name));

  QPushButton* dont_install_button =
      prompt.addButton(QObject::tr("Don't Install"), QMessageBox::RejectRole);
  QPushButton* install_button = prompt.addButton(QObject::tr("Install"), QMessageBox::AcceptRole);
  prompt.setDefaultButton(install_button);
  prompt.setEscapeButton(dont_install_button);

  prompt.exec();
  if (prompt.clickedButton() != install_button)
    return true;

  QString last_error;
  const std::vector<std::string> targets = GetInstallTargets(bundle_path);
  for (const std::string& destination_path : targets)
  {
    if (!InstallBundle(bundle_path, destination_path, &last_error))
      continue;

    if (!RelaunchInstalledApp(destination_path))
    {
      ModalMessageBox::critical(
          nullptr, QObject::tr("Install Failed"),
          QObject::tr("%1 was installed, but couldn't be launched.").arg(app_name));
      return true;
    }

    return false;
  }

  ModalMessageBox::critical(nullptr, QObject::tr("Install Failed"),
                            QObject::tr("Couldn't install %1.").arg(app_name), QMessageBox::Ok,
                            QMessageBox::NoButton, Qt::ApplicationModal, last_error);
  return true;
#endif
}
}  // namespace QtUtils
