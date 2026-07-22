// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <vector>

#include <QPointer>
#include <QString>
#include <QWidget>

class QEvent;
class QLabel;
class QLineEdit;
class QListWidget;
class QStackedWidget;
class QToolButton;

class SettingsSearchWidget final : public QWidget
{
  Q_OBJECT
public:
  explicit SettingsSearchWidget(QListWidget* navigation_list, QStackedWidget* stacked_panes,
                                QStackedWidget* content_stack, QWidget* parent = nullptr);

  void AddPane(const QString& name);
  void Reset();

private:
  struct SearchResult
  {
    int pane_index;
    QPointer<QWidget> widget;
    bool can_highlight;
  };

  struct PaneSearchResult
  {
    int match_count = 0;
    bool name_matches = false;
  };

  struct SearchResults
  {
    std::vector<SearchResult> items;
    std::vector<PaneSearchResult> panes;
  };

  bool eventFilter(QObject* watched, QEvent* event) override;

  void CreateWidgets();
  void ConnectWidgets();

  void ApplySearchFilter();
  SearchResults FindSearchResults(const QString& search_term) const;
  int ApplyNavigationFilter(const QString& search_term, const SearchResults& results);
  void ClearSearchHighlights();
  void NavigateSearchResults(int direction);
  void UpdateCurrentPaneHighlights();
  void UpdateSearchResultControls();

  QListWidget* m_navigation_list;
  QStackedWidget* m_stacked_panes;
  QStackedWidget* m_content_stack;
  QLineEdit* m_search_bar = nullptr;
  QWidget* m_search_navigation_widget = nullptr;
  QToolButton* m_search_previous_button = nullptr;
  QToolButton* m_search_next_button = nullptr;
  QLabel* m_search_results_label = nullptr;
  std::vector<QString> m_pane_names;
  std::vector<SearchResult> m_search_results;
  int m_current_search_result_index = -1;
  std::vector<QPointer<QWidget>> m_highlighted_widgets;
};
