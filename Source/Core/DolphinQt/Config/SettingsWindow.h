// Copyright 2015 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QPointer>
#include <vector>

#include <QDialog>

class QStackedWidget;
class QListWidget;
class QLineEdit;
class QLabel;
class QToolButton;
class QWidget;
class MainWindow;
class QEvent;

// A settings window with a QListWidget to switch between panes of a QStackedWidget.
class StackedSettingsWindow : public QDialog
{
  Q_OBJECT
public:
  explicit StackedSettingsWindow(QWidget* parent = nullptr);

  void ActivatePane(int index);

protected:
  void AddPane(QWidget*, const QString& name);

  // Adds a scrollable Pane.
  void AddWrappedPane(QWidget*, const QString& name);

  // For derived classes to call after they create their settings panes.
  void OnDoneCreatingPanes();

  void changeEvent(QEvent* event) override;
  bool eventFilter(QObject* watched, QEvent* event) override;
  void done(int result) override;

private:
  struct SearchResult
  {
    int pane_index;
    QPointer<QWidget> widget;
  };

  void ApplySearchFilter();
  void UpdateCurrentPaneHighlights();
  void ClearSearchHighlights();
  void UpdateSearchResultControls();
  void NavigateSearchResults(int direction);

  void UpdateNavigationListStyle();
  void UpdateSearchNavigationIcons();

  QStackedWidget* m_stacked_panes = nullptr;
  QListWidget* m_navigation_list = nullptr;
  QLineEdit* m_search_bar = nullptr;
  QWidget* m_search_navigation_widget = nullptr;
  QToolButton* m_search_previous_button = nullptr;
  QToolButton* m_search_next_button = nullptr;
  QLabel* m_search_results_label = nullptr;
  std::vector<QString> m_pane_names;
  std::vector<SearchResult> m_search_results;
  int m_current_search_result_index = -1;
  std::vector<QPointer<QWidget>> m_first_match_widgets;
  std::vector<QPointer<QWidget>> m_highlighted_widgets;
  bool m_handling_theme_change = false;
};

enum class SettingsWindowPaneIndex : int
{
  General = 0,
  Graphics,
  Controllers,
  Interface,
  OnScreenDisplay,
  Audio,
  Paths,
  GameCube,
  Wii,
  Triforce,
  Advanced,
};

class SettingsWindow final : public StackedSettingsWindow
{
  Q_OBJECT
public:
  explicit SettingsWindow(MainWindow* parent);

  void SelectPane(SettingsWindowPaneIndex);
};
