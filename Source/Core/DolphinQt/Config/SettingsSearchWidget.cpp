// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/SettingsSearchWidget.h"

#include <algorithm>
#include <utility>

#include <QComboBox>
#include <QEvent>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QIcon>
#include <QItemSelectionModel>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QScrollArea>
#include <QShortcut>
#include <QSize>
#include <QStackedWidget>
#include <QStyle>
#include <QTabWidget>
#include <QToolButton>
#include <QVariant>

namespace
{
constexpr auto SEARCH_MATCH_PROPERTY = "settingsSearchMatch";
constexpr auto SEARCH_CURRENT_PROPERTY = "settingsSearchCurrent";

bool ObjectMatchesSearchTerm(const QObject* object, const QString& search_term)
{
  const auto property_matches = [object, &search_term](const char* property_name) {
    const QVariant property = object->property(property_name);
    return property.canConvert<QString>() &&
           property.toString().contains(search_term, Qt::CaseInsensitive);
  };

  return property_matches("text") || property_matches("title") || property_matches("windowTitle") ||
         property_matches("placeholderText");
}

void CollectMatchingWidgets(const QObject* root, const QString& search_term,
                            std::vector<QWidget*>* matches)
{
  if (search_term.isEmpty())
    return;

  if (qobject_cast<const QComboBox*>(root))
    return;

  if (const auto* tab_widget = qobject_cast<const QTabWidget*>(root))
  {
    for (int i = 0; i < tab_widget->count(); ++i)
    {
      if (!tab_widget->tabText(i).contains(search_term, Qt::CaseInsensitive))
        continue;

      if (QWidget* const page = tab_widget->widget(i);
          page && std::find(matches->begin(), matches->end(), page) == matches->end())
      {
        matches->push_back(page);
      }
    }

    for (int i = 0; i < tab_widget->count(); ++i)
    {
      if (QWidget* const page = tab_widget->widget(i))
        CollectMatchingWidgets(page, search_term, matches);
    }
    return;
  }

  const bool object_matches = ObjectMatchesSearchTerm(root, search_term);
  const bool is_group_box = qobject_cast<const QGroupBox*>(root);
  if (object_matches && !is_group_box)
  {
    if (auto* widget = qobject_cast<QWidget*>(const_cast<QObject*>(root));
        widget && !widget->isHidden() &&
        std::find(matches->begin(), matches->end(), widget) == matches->end())
    {
      matches->push_back(widget);
    }
  }

  const std::size_t matches_before_children = matches->size();
  for (const QObject* child : root->children())
    CollectMatchingWidgets(child, search_term, matches);

  // A group title often repeats the label of a setting inside it. Prefer the concrete setting in
  // that case, while still making group titles searchable when no child matches.
  if (object_matches && is_group_box && matches->size() == matches_before_children)
  {
    if (auto* widget = qobject_cast<QWidget*>(const_cast<QObject*>(root));
        widget && !widget->isHidden() &&
        std::find(matches->begin(), matches->end(), widget) == matches->end())
    {
      matches->push_back(widget);
    }
  }
}

void SetHighlightState(QWidget* widget, bool matches, bool current)
{
  widget->setProperty(SEARCH_MATCH_PROPERTY, matches);
  widget->setProperty(SEARCH_CURRENT_PROPERTY, current);
  widget->style()->unpolish(widget);
  widget->style()->polish(widget);
  widget->update();
}

void EnsureWidgetIsVisible(QWidget* widget)
{
  if (!widget)
    return;

  for (QWidget* ancestor = widget; ancestor; ancestor = ancestor->parentWidget())
  {
    auto* const tab_widget = qobject_cast<QTabWidget*>(ancestor);
    if (!tab_widget)
      continue;

    for (QWidget* page = widget; page && page != tab_widget; page = page->parentWidget())
    {
      const int tab_index = tab_widget->indexOf(page);
      if (tab_index != -1)
      {
        tab_widget->setCurrentIndex(tab_index);
        break;
      }
    }
  }

  QWidget* current = widget;
  while (current)
  {
    if (auto* const scroll_area = qobject_cast<QScrollArea*>(current))
    {
      scroll_area->ensureWidgetVisible(widget, 16, 16);
      break;
    }
    current = current->parentWidget();
  }
}

QString GetHighlightStyleSheet()
{
  return QStringLiteral(
      "QLabel[settingsSearchMatch=\"true\"] { "
      "background: palette(alternate-base); border: 1px solid palette(highlight); "
      "border-radius: 4px; padding: 1px 4px; } "
      "QCheckBox[settingsSearchMatch=\"true\"], "
      "QRadioButton[settingsSearchMatch=\"true\"] { "
      "background: palette(alternate-base); border-radius: 4px; } "
      "QGroupBox[settingsSearchMatch=\"true\"]::title { "
      "background: palette(alternate-base); border: 1px solid palette(highlight); "
      "border-radius: 4px; padding: 1px 4px; } "
      "QLabel[settingsSearchCurrent=\"true\"], "
      "QCheckBox[settingsSearchCurrent=\"true\"], "
      "QRadioButton[settingsSearchCurrent=\"true\"], "
      "QGroupBox[settingsSearchCurrent=\"true\"]::title { "
      "background: palette(highlight); color: palette(highlighted-text); } "
      "QPushButton[settingsSearchMatch=\"true\"], "
      "QLineEdit[settingsSearchMatch=\"true\"], "
      "QSpinBox[settingsSearchMatch=\"true\"], "
      "QDoubleSpinBox[settingsSearchMatch=\"true\"] { "
      "border: 1px solid palette(highlight); border-radius: 4px; } "
      "QPushButton[settingsSearchCurrent=\"true\"], "
      "QLineEdit[settingsSearchCurrent=\"true\"], "
      "QSpinBox[settingsSearchCurrent=\"true\"], "
      "QDoubleSpinBox[settingsSearchCurrent=\"true\"] { "
      "border: 2px solid palette(highlight); border-radius: 4px; }");
}

QString GetSearchBarStyleSheet()
{
  return QStringLiteral(
      "QFrame#SettingsSearchFrame { "
      "background: palette(base); border: 1px solid palette(mid); border-radius: 6px; } "
      "QFrame#SettingsSearchFrame QLineEdit { "
      "background: transparent; border: none; padding: 0px; } "
      "QFrame#SettingsSearchFrame QToolButton { "
      "background: transparent; border: none; border-radius: 4px; padding: 2px; } "
      "QFrame#SettingsSearchFrame QToolButton:hover:!disabled { "
      "background: palette(alternate-base); }");
}

}  // namespace

SettingsSearchWidget::SettingsSearchWidget(QListWidget* navigation_list,
                                           QStackedWidget* stacked_panes,
                                           QStackedWidget* content_stack, QWidget* parent)
    : QWidget{parent}, m_navigation_list{navigation_list}, m_stacked_panes{stacked_panes},
      m_content_stack{content_stack}
{
  CreateWidgets();
  ConnectWidgets();
  UpdateSearchResultControls();
}

void SettingsSearchWidget::CreateWidgets()
{
  setStyleSheet(GetSearchBarStyleSheet());
  m_stacked_panes->setStyleSheet(GetHighlightStyleSheet());

  auto* const layout = new QHBoxLayout(this);
  layout->setContentsMargins(14, 12, 14, 8);
  layout->setSpacing(6);

  auto* const search_frame = new QFrame;
  search_frame->setObjectName(QStringLiteral("SettingsSearchFrame"));
  auto* const search_layout = new QHBoxLayout(search_frame);
  search_layout->setContentsMargins(8, 1, 4, 1);
  search_layout->setSpacing(4);
  layout->addWidget(search_frame, 1);

  m_search_bar = new QLineEdit;
  m_search_bar->setPlaceholderText(tr("Search all sections"));
  m_search_bar->setToolTip(tr("Searches across all settings sections"));
  m_search_bar->setClearButtonEnabled(true);
  search_frame->setMinimumHeight(m_search_bar->sizeHint().height() + 6);
  QIcon search_icon = QIcon::fromTheme(QStringLiteral("edit-find"));
  if (!search_icon.isNull())
    m_search_bar->addAction(search_icon, QLineEdit::LeadingPosition);
  search_layout->addWidget(m_search_bar, 1);

  m_search_navigation_widget = new QWidget;
  auto* const navigation_layout = new QHBoxLayout(m_search_navigation_widget);
  navigation_layout->setContentsMargins(0, 0, 0, 0);
  navigation_layout->setSpacing(0);

  m_search_results_label = new QLabel;
  m_search_results_label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
  m_search_results_label->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
  m_search_results_label->setStyleSheet(QStringLiteral("color: palette(window-text);"));
  navigation_layout->addWidget(m_search_results_label);
  navigation_layout->addSpacing(6);

  m_search_previous_button = new QToolButton;
  m_search_previous_button->setAutoRaise(true);
  m_search_previous_button->setFocusPolicy(Qt::NoFocus);
  m_search_previous_button->setToolTip(tr("Previous match"));
  m_search_previous_button->setIcon(style()->standardIcon(QStyle::SP_ArrowUp));
  m_search_previous_button->setIconSize(QSize{14, 14});
  navigation_layout->addWidget(m_search_previous_button);

  m_search_next_button = new QToolButton;
  m_search_next_button->setAutoRaise(true);
  m_search_next_button->setFocusPolicy(Qt::NoFocus);
  m_search_next_button->setToolTip(tr("Next match"));
  m_search_next_button->setIcon(style()->standardIcon(QStyle::SP_ArrowDown));
  m_search_next_button->setIconSize(QSize{14, 14});
  navigation_layout->addWidget(m_search_next_button);

  m_search_navigation_widget->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Preferred);
  layout->addWidget(m_search_navigation_widget, 0, Qt::AlignVCenter);
}

void SettingsSearchWidget::ConnectWidgets()
{
  connect(m_search_bar, &QLineEdit::textChanged, this, &SettingsSearchWidget::ApplySearchFilter);
  connect(m_navigation_list, &QListWidget::currentRowChanged, this,
          &SettingsSearchWidget::UpdateCurrentPaneHighlights);
  connect(m_search_previous_button, &QToolButton::clicked, this,
          [this] { NavigateSearchResults(-1); });
  connect(m_search_next_button, &QToolButton::clicked, this, [this] { NavigateSearchResults(1); });

  m_search_bar->installEventFilter(this);
  auto* const find_shortcut = new QShortcut(QKeySequence::Find, this);
  connect(find_shortcut, &QShortcut::activated, this, [this] {
    m_search_bar->setFocus();
    m_search_bar->selectAll();
  });
}

void SettingsSearchWidget::AddPane(const QString& name)
{
  m_pane_names.push_back(name);
  ApplySearchFilter();
}

void SettingsSearchWidget::Reset()
{
  if (!m_search_bar->text().isEmpty())
    m_search_bar->clear();
}

bool SettingsSearchWidget::eventFilter(QObject* watched, QEvent* event)
{
  if (watched == m_search_bar && event && event->type() == QEvent::KeyPress)
  {
    const auto* const key_event = static_cast<QKeyEvent*>(event);
    if (key_event->key() == Qt::Key_Return || key_event->key() == Qt::Key_Enter)
    {
      NavigateSearchResults(key_event->modifiers().testFlag(Qt::ShiftModifier) ? -1 : 1);
      return true;
    }

    if (key_event->key() == Qt::Key_Escape && !m_search_bar->text().isEmpty())
    {
      m_search_bar->clear();
      return true;
    }
  }

  return QWidget::eventFilter(watched, event);
}

void SettingsSearchWidget::ApplySearchFilter()
{
  const QString search_term = m_search_bar->text().trimmed();
  SearchResults results = FindSearchResults(search_term);

  m_search_results = std::move(results.items);
  m_current_search_result_index = -1;
  if (!search_term.isEmpty() && !m_search_results.empty())
    m_current_search_result_index = 0;

  ClearSearchHighlights();
  const int first_visible_row = ApplyNavigationFilter(search_term, results);

  if (first_visible_row == -1)
  {
    m_content_stack->setCurrentIndex(1);
    m_navigation_list->clearSelection();
    UpdateSearchResultControls();
    return;
  }

  m_content_stack->setCurrentIndex(0);

  int target_row = -1;
  if (m_current_search_result_index >= 0 &&
      m_current_search_result_index < static_cast<int>(m_search_results.size()))
  {
    target_row = m_search_results[m_current_search_result_index].pane_index;
  }

  const int current_row = m_navigation_list->currentRow();
  if (target_row < 0 || target_row >= m_navigation_list->count() ||
      m_navigation_list->item(target_row)->isHidden())
  {
    if (current_row < 0 || m_navigation_list->item(current_row)->isHidden())
      target_row = first_visible_row;
    else
      target_row = current_row;
  }

  m_navigation_list->setCurrentRow(target_row, QItemSelectionModel::ClearAndSelect);

  UpdateCurrentPaneHighlights();
}

SettingsSearchWidget::SearchResults
SettingsSearchWidget::FindSearchResults(const QString& search_term) const
{
  const int pane_count = m_navigation_list->count();
  SearchResults results;
  results.panes.resize(pane_count);

  if (search_term.isEmpty())
    return results;

  for (int i = 0; i < pane_count; ++i)
  {
    const bool pane_name_matches = i < static_cast<int>(m_pane_names.size()) &&
                                   m_pane_names[i].contains(search_term, Qt::CaseInsensitive);
    results.panes[i].name_matches = pane_name_matches;

    QWidget* const pane = m_stacked_panes->widget(i);
    if (!pane)
      continue;

    std::vector<QWidget*> pane_matches;
    CollectMatchingWidgets(pane, search_term, &pane_matches);
    results.panes[i].match_count = static_cast<int>(pane_matches.size());

    for (QWidget* const widget : pane_matches)
      results.items.push_back({i, widget, true});

    if (pane_name_matches && pane_matches.empty())
      results.items.push_back({i, pane, false});
  }

  return results;
}

int SettingsSearchWidget::ApplyNavigationFilter(const QString& search_term,
                                                const SearchResults& results)
{
  int first_visible_row = -1;
  for (int i = 0; i < m_navigation_list->count(); ++i)
  {
    const PaneSearchResult& pane_result = results.panes[i];
    const bool matches =
        search_term.isEmpty() || pane_result.name_matches || pane_result.match_count > 0;

    QListWidgetItem* const item = m_navigation_list->item(i);
    item->setHidden(!matches);

    const QString pane_name =
        i < static_cast<int>(m_pane_names.size()) ? m_pane_names[i] : QString{};
    item->setText(search_term.isEmpty() || pane_result.match_count == 0 ?
                      QStringLiteral("  %1  ").arg(pane_name) :
                      QStringLiteral("  %1 (%2)  ").arg(pane_name).arg(pane_result.match_count));

    if (matches && first_visible_row == -1)
      first_visible_row = i;
  }

  return first_visible_row;
}

void SettingsSearchWidget::ClearSearchHighlights()
{
  for (const QPointer<QWidget>& widget : m_highlighted_widgets)
  {
    if (!widget)
      continue;

    SetHighlightState(widget, false, false);
  }
  m_highlighted_widgets.clear();
}

void SettingsSearchWidget::NavigateSearchResults(int direction)
{
  if (direction == 0 || m_search_results.empty())
    return;

  const int result_count = static_cast<int>(m_search_results.size());
  int index = m_current_search_result_index;
  if (index < 0 || index >= result_count)
    index = (direction > 0) ? -1 : 0;

  for (int i = 0; i < result_count; ++i)
  {
    index = (index + direction + result_count) % result_count;
    const SearchResult& result = m_search_results[index];
    if (!result.widget || result.pane_index < 0 || result.pane_index >= m_navigation_list->count())
      continue;
    if (m_navigation_list->item(result.pane_index)->isHidden())
      continue;

    m_current_search_result_index = index;
    if (m_navigation_list->currentRow() != result.pane_index)
      m_navigation_list->setCurrentRow(result.pane_index);
    else
      UpdateCurrentPaneHighlights();
    return;
  }
}

void SettingsSearchWidget::UpdateCurrentPaneHighlights()
{
  ClearSearchHighlights();

  const QString search_term = m_search_bar->text().trimmed();
  const int current_row = m_navigation_list->currentRow();
  const bool can_highlight =
      !search_term.isEmpty() && current_row >= 0 && current_row < m_stacked_panes->count() &&
      !m_navigation_list->item(current_row)->isHidden() && m_stacked_panes->widget(current_row);
  if (!can_highlight)
  {
    UpdateSearchResultControls();
    return;
  }

  QWidget* current_match = nullptr;
  if (m_current_search_result_index >= 0 &&
      m_current_search_result_index < static_cast<int>(m_search_results.size()))
  {
    const SearchResult& result = m_search_results[m_current_search_result_index];
    if (result.pane_index == current_row)
      current_match = result.widget;
  }

  if (!current_match)
  {
    const auto current_it = std::find_if(m_search_results.cbegin(), m_search_results.cend(),
                                         [current_row](const SearchResult& result) {
                                           return result.pane_index == current_row && result.widget;
                                         });
    if (current_it != m_search_results.cend())
    {
      current_match = current_it->widget;
      m_current_search_result_index =
          static_cast<int>(std::distance(m_search_results.cbegin(), current_it));
    }
  }

  for (const SearchResult& result : m_search_results)
  {
    if (result.pane_index != current_row || !result.can_highlight || !result.widget)
      continue;

    SetHighlightState(result.widget, true, result.widget == current_match);
    m_highlighted_widgets.push_back(result.widget);
  }

  EnsureWidgetIsVisible(current_match);
  UpdateSearchResultControls();
}

void SettingsSearchWidget::UpdateSearchResultControls()
{
  const QString search_term = m_search_bar->text().trimmed();
  const bool search_is_active = !search_term.isEmpty();
  const int result_count = search_is_active ? static_cast<int>(m_search_results.size()) : 0;
  const bool has_results = result_count > 0;

  m_search_navigation_widget->setVisible(search_is_active);
  m_search_previous_button->setEnabled(has_results);
  m_search_next_button->setEnabled(has_results);

  if (!search_is_active)
  {
    m_search_results_label->clear();
    return;
  }

  if (!has_results)
  {
    m_search_results_label->setText(tr("0 results"));
    return;
  }

  const int current_result =
      (m_current_search_result_index >= 0 && m_current_search_result_index < result_count) ?
          (m_current_search_result_index + 1) :
          1;
  m_search_results_label->setText(tr("%1 of %2").arg(current_result).arg(result_count));
}
