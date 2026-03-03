// Copyright 2016 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/SettingsWindow.h"

#include <algorithm>
#include <utility>

#include <QApplication>
#include <QColor>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QEvent>
#include <QFontMetrics>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QIcon>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPainter>
#include <QPalette>
#include <QScrollArea>
#include <QStackedWidget>
#include <QStyle>
#include <QTabWidget>
#include <QToolButton>
#include <QVBoxLayout>

#include "DolphinQt/Config/ControllersPane.h"
#include "DolphinQt/Config/Graphics/GraphicsPane.h"
#include "DolphinQt/MainWindow.h"
#include "DolphinQt/QtUtils/QtUtils.h"
#include "DolphinQt/QtUtils/WrapInScrollArea.h"
#include "DolphinQt/Settings.h"
#include "DolphinQt/Settings/AdvancedPane.h"
#include "DolphinQt/Settings/AudioPane.h"
#include "DolphinQt/Settings/GameCubePane.h"
#include "DolphinQt/Settings/GeneralPane.h"
#include "DolphinQt/Settings/InterfacePane.h"
#include "DolphinQt/Settings/OnScreenDisplayPane.h"
#include "DolphinQt/Settings/PathPane.h"
#include "DolphinQt/Settings/TriforcePane.h"
#include "DolphinQt/Settings/WiiPane.h"

namespace
{
constexpr int MIN_SETTINGS_SEARCH_LENGTH = 2;

bool ObjectMatchesSearchTerm(const QObject* object, const QString& search_term)
{
  const auto property_matches = [object, &search_term](const char* property_name) {
    const QVariant property = object->property(property_name);
    return property.canConvert<QString>() &&
           property.toString().contains(search_term, Qt::CaseInsensitive);
  };

  if (property_matches("text") || property_matches("title") || property_matches("windowTitle") ||
      property_matches("placeholderText"))
  {
    return true;
  }

  return false;
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
  }

  if (ObjectMatchesSearchTerm(root, search_term))
  {
    if (auto* widget = qobject_cast<QWidget*>(const_cast<QObject*>(root));
        widget && !qobject_cast<QGroupBox*>(widget) &&
        std::find(matches->begin(), matches->end(), widget) == matches->end())
    {
      matches->push_back(widget);
    }
  }

  for (const QObject* child : root->children())
    CollectMatchingWidgets(child, search_term, matches);
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

QIcon CreateSearchNavigationIcon(Qt::ArrowType arrow_type, const QColor& active_color,
                                 const QColor& disabled_color)
{
  const auto draw_icon = [arrow_type](const QColor& color) {
    QPixmap pixmap{QSize{14, 14}};
    pixmap.fill(Qt::transparent);

    QPainter painter{&pixmap};
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QPen{color, 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin});

    if (arrow_type == Qt::LeftArrow)
    {
      painter.drawLine(QPointF{9.5, 2.5}, QPointF{4.5, 7.0});
      painter.drawLine(QPointF{4.5, 7.0}, QPointF{9.5, 11.5});
    }
    else
    {
      painter.drawLine(QPointF{4.5, 2.5}, QPointF{9.5, 7.0});
      painter.drawLine(QPointF{9.5, 7.0}, QPointF{4.5, 11.5});
    }

    return pixmap;
  };

  QIcon icon;
  icon.addPixmap(draw_icon(active_color), QIcon::Normal, QIcon::Off);
  icon.addPixmap(draw_icon(active_color), QIcon::Active, QIcon::Off);
  icon.addPixmap(draw_icon(disabled_color), QIcon::Disabled, QIcon::Off);
  return icon;
}

}  // namespace

StackedSettingsWindow::StackedSettingsWindow(QWidget* parent) : QDialog{parent}
{
  const QString search_match_color = QStringLiteral("#EBCB77");
  const QString search_current_color = QStringLiteral("#E0A21F");
  const QString settings_search_style =
      QStringLiteral(
          "QLabel[settingsSearchMatch=\"true\"] { "
          "background-color: %1; border-radius: 8px; padding: 1px 6px; } "
          "QCheckBox[settingsSearchMatch=\"true\"], "
          "QRadioButton[settingsSearchMatch=\"true\"] { "
          "background-color: %1; border-radius: 8px; } "
          "QLabel[settingsSearchCurrent=\"true\"], "
          "QCheckBox[settingsSearchCurrent=\"true\"], "
          "QRadioButton[settingsSearchCurrent=\"true\"] { "
          "background-color: %2; border-radius: 8px; } "
          "QPushButton[settingsSearchMatch=\"true\"], "
          "QLineEdit[settingsSearchMatch=\"true\"], "
          "QSpinBox[settingsSearchMatch=\"true\"], "
          "QDoubleSpinBox[settingsSearchMatch=\"true\"] { "
          "background-color: transparent; border: 2px solid %1; "
          "border-radius: 8px; } "
          "QPushButton[settingsSearchCurrent=\"true\"], "
          "QLineEdit[settingsSearchCurrent=\"true\"], "
          "QSpinBox[settingsSearchCurrent=\"true\"], "
          "QDoubleSpinBox[settingsSearchCurrent=\"true\"] { "
          "background-color: transparent; border: 2px solid %2; "
          "border-radius: 8px; } "
          "QLineEdit#SettingsSearchInput { "
          "border: 1px solid palette(mid); border-radius: 7px; padding: 4px 8px; } "
          "QLineEdit#SettingsSearchInput:focus { border: 1px solid palette(highlight); } "
          "QToolButton#SettingsSearchNavButton { "
          "min-width: 22px; min-height: 22px; border: 1px solid palette(mid); "
          "border-radius: 6px; padding: 0px; background: palette(base); } "
          "QToolButton#SettingsSearchNavButton:hover:!disabled { "
          "border-color: palette(highlight); } "
          "QToolButton#SettingsSearchNavButton:disabled { color: palette(mid); }")
          .arg(search_match_color, search_current_color);

  // This eliminates the ugly line between the title bar and window contents with KDE Plasma.
  const QString dialog_border_fix_style = QStringLiteral("QDialog { border: none; }");
  const QString full_style_sheet = dialog_border_fix_style + settings_search_style;
  setStyleSheet(full_style_sheet);

  auto* const layout = new QVBoxLayout{this};
  // Eliminate padding around layouts.
  layout->setContentsMargins(QMargins{});
  layout->setSpacing(0);

  // Calculated value for the padding in our list items.
  const int list_item_padding = style()->pixelMetric(QStyle::PM_LayoutLeftMargin) / 2;

  m_navigation_list = new QListWidget;

  // Ensure list doesn't grow horizontally and is not resized smaller than its contents.
  m_navigation_list->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Minimum);
  m_navigation_list->setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContents);

  // FYI: "base" is the window color on Windows and "alternate-base" is very high contrast on macOS.
  const auto* const list_background =
#if !defined(__APPLE__)
      "palette(alternate-base)";
#else
      "palette(base)";
#endif

  m_navigation_list->setStyleSheet(
      QString::fromUtf8(
          // Remove border around entire widget and adjust background color.
          "QListWidget { border: 0; background: %1; } "
          // Note: padding-left is broken unless border is set, which then breaks colors.
          // see: https://bugreports.qt.io/browse/QTBUG-122698
          "QListWidget::item { padding-top: %2px; padding-bottom: %2px; } "
          // Maintain selected item color when unfocused.
          "QListWidget::item:selected { background: palette(highlight); "
          // Prevent text color change on focus loss.
          "color: palette(highlighted-text); "
          "} "
          // Remove ugly dotted outline on selected row (Windows and GNOME).
          "* { outline: none; } ")
          .arg(QString::fromUtf8(list_background))
          .arg(list_item_padding));

  UpdateNavigationListStyle();

  auto* const search_frame = new QFrame;
  auto* const search_layout = new QHBoxLayout(search_frame);
  search_layout->setContentsMargins(14, 12, 14, 8);
  search_layout->setSpacing(8);

  m_search_bar = new QLineEdit;
  m_search_bar->setObjectName(QStringLiteral("SettingsSearchInput"));
  m_search_bar->setPlaceholderText(tr("Search all sections"));
  m_search_bar->setToolTip(tr("Searches across all settings sections"));
  m_search_bar->setClearButtonEnabled(true);
  m_search_bar->setMinimumHeight(m_search_bar->sizeHint().height() + 6);
  QIcon search_icon = QIcon::fromTheme(QStringLiteral("edit-find"));
  if (search_icon.isNull())
    search_icon = style()->standardIcon(QStyle::SP_FileDialogContentsView);
  m_search_bar->addAction(search_icon, QLineEdit::LeadingPosition);
  search_layout->addWidget(m_search_bar, 1);

  m_search_navigation_widget = new QWidget;
  auto* const navigation_layout = new QHBoxLayout(m_search_navigation_widget);
  navigation_layout->setContentsMargins(0, 0, 0, 0);
  navigation_layout->setSpacing(4);

  m_search_results_label = new QLabel;
  m_search_results_label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
  const QFontMetrics label_metrics{m_search_results_label->font()};
  const int results_label_min_width =
      std::max(label_metrics.horizontalAdvance(tr("0 results")),
               label_metrics.horizontalAdvance(tr("%1 of %2").arg(100).arg(100)));
  m_search_results_label->setMinimumWidth(results_label_min_width);
  m_search_results_label->setStyleSheet(QStringLiteral("color: palette(window-text);"));
  navigation_layout->addWidget(m_search_results_label);

  m_search_previous_button = new QToolButton;
  m_search_previous_button->setObjectName(QStringLiteral("SettingsSearchNavButton"));
  m_search_previous_button->setAutoRaise(true);
  m_search_previous_button->setFocusPolicy(Qt::NoFocus);
  m_search_previous_button->setToolTip(tr("Previous match"));
  navigation_layout->addWidget(m_search_previous_button);

  m_search_next_button = new QToolButton;
  m_search_next_button->setObjectName(QStringLiteral("SettingsSearchNavButton"));
  m_search_next_button->setAutoRaise(true);
  m_search_next_button->setFocusPolicy(Qt::NoFocus);
  m_search_next_button->setToolTip(tr("Next match"));
  navigation_layout->addWidget(m_search_next_button);

  UpdateSearchNavigationIcons();

  m_search_navigation_widget->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Preferred);
  search_layout->addWidget(m_search_navigation_widget, 0, Qt::AlignVCenter);

  layout->addWidget(search_frame);

  auto* const content_layout = new QHBoxLayout;
  content_layout->setContentsMargins(QMargins{});
  content_layout->setSpacing(0);
  layout->addLayout(content_layout);

  content_layout->addWidget(m_navigation_list);

  auto* const right_side = new QVBoxLayout;
  right_side->setContentsMargins(14, 0, 14, 0);
  right_side->setSpacing(10);
  content_layout->addLayout(right_side);

  m_stacked_panes = new QStackedWidget;

  right_side->addWidget(m_stacked_panes);

  // The QFrame gives us some padding around the button.
  auto* const button_frame = new QFrame;
  auto* const button_layout = new QGridLayout{button_frame};
  auto* const button_box = new QDialogButtonBox(QDialogButtonBox::Close);
  right_side->addWidget(button_frame);
  button_layout->addWidget(button_box);

  connect(button_box, &QDialogButtonBox::rejected, this, &QDialog::reject);

  connect(m_navigation_list, &QListWidget::currentRowChanged, m_stacked_panes,
          &QStackedWidget::setCurrentIndex);
  connect(m_search_bar, &QLineEdit::textChanged, this, [this] { ApplySearchFilter(); });
  connect(m_navigation_list, &QListWidget::currentRowChanged, this,
          [this] { UpdateCurrentPaneHighlights(); });
  connect(m_search_previous_button, &QToolButton::clicked, this,
          [this] { NavigateSearchResults(-1); });
  connect(m_search_next_button, &QToolButton::clicked, this, [this] { NavigateSearchResults(1); });

  m_search_bar->installEventFilter(this);
  UpdateSearchResultControls();
}

void StackedSettingsWindow::OnDoneCreatingPanes()
{
  ApplySearchFilter();
  // Make sure the first item is actually selected by default.
  ActivatePane(0);
  // Take on the preferred size.
  QtUtils::AdjustSizeWithinScreen(this);
}

void StackedSettingsWindow::changeEvent(QEvent* event)
{
  QDialog::changeEvent(event);

  const auto type = event->type();

  const bool palette_changed = type == QEvent::PaletteChange;
  const bool application_palette_changed = type == QEvent::ApplicationPaletteChange;
  const bool style_changed = type == QEvent::StyleChange;
  const bool theme_event = type == QEvent::ThemeChange;

  const bool theme_changed = application_palette_changed || theme_event;

  if (theme_changed && !m_handling_theme_change)
  {
    m_handling_theme_change = true;
    Settings::Instance().ApplyStyle();
    // Ensure the dialog and its children adopt the new system palette.
    setPalette(qApp->palette());
    Settings::Instance().TriggerThemeChanged();
    m_handling_theme_change = false;
  }

  if (palette_changed || application_palette_changed || style_changed || theme_event)
  {
    UpdateNavigationListStyle();
    UpdateSearchNavigationIcons();
  }
}

bool StackedSettingsWindow::eventFilter(QObject* watched, QEvent* event)
{
  if (watched == m_search_bar && event && event->type() == QEvent::KeyPress)
  {
    const auto* const key_event = static_cast<QKeyEvent*>(event);
    if (key_event->key() == Qt::Key_Return || key_event->key() == Qt::Key_Enter)
    {
      NavigateSearchResults(1);
      return true;
    }
  }

  return QDialog::eventFilter(watched, event);
}

void StackedSettingsWindow::done(int result)
{
  if (m_search_bar && !m_search_bar->text().isEmpty())
    m_search_bar->clear();

  QDialog::done(result);
}

void StackedSettingsWindow::UpdateNavigationListStyle()
{
  if (!m_navigation_list)
    return;

  QPalette list_palette = m_navigation_list->palette();
  const QPalette app_palette = qApp->palette();

  QColor highlight_color = app_palette.color(QPalette::Active, QPalette::Highlight);
  QColor highlighted_text = app_palette.color(QPalette::Active, QPalette::HighlightedText);

#if defined(__APPLE__)
  const bool is_dark_theme = Settings::Instance().IsThemeDark();
  // The default macOS accent is quite light in our list; darken it for readability in light mode.
  if (!is_dark_theme)
  {
    highlight_color = highlight_color.darker(130);
    highlighted_text = QColor(Qt::white);
  }
#endif

  for (const QPalette::ColorGroup group : {QPalette::Active, QPalette::Inactive})
  {
    list_palette.setColor(group, QPalette::Base, app_palette.color(group, QPalette::Base));
    list_palette.setColor(group, QPalette::AlternateBase,
                          app_palette.color(group, QPalette::AlternateBase));
    list_palette.setColor(group, QPalette::Highlight, highlight_color);
    list_palette.setColor(group, QPalette::HighlightedText, highlighted_text);
  }

  m_navigation_list->setPalette(list_palette);
}

void StackedSettingsWindow::UpdateSearchNavigationIcons()
{
  if (!m_search_previous_button || !m_search_next_button)
    return;

  const QPalette palette = qApp->palette();
  const QColor active_color = palette.color(QPalette::Active, QPalette::WindowText);
  const QColor disabled_color = palette.color(QPalette::Disabled, QPalette::WindowText);

  m_search_previous_button->setIcon(
      CreateSearchNavigationIcon(Qt::LeftArrow, active_color, disabled_color));
  m_search_next_button->setIcon(
      CreateSearchNavigationIcon(Qt::RightArrow, active_color, disabled_color));
}

void StackedSettingsWindow::AddPane(QWidget* widget, const QString& name)
{
  m_stacked_panes->addWidget(widget);
  m_pane_names.push_back(name);
  m_first_match_widgets.emplace_back(nullptr);
  // Pad the left and right of each item.
  m_navigation_list->addItem(QStringLiteral("  %1  ").arg(name));
  ApplySearchFilter();
}

void StackedSettingsWindow::AddWrappedPane(QWidget* widget, const QString& name)
{
  AddPane(GetWrappedWidget(widget), name);
}

void StackedSettingsWindow::ActivatePane(int index)
{
  m_navigation_list->setCurrentRow(index);
}

void StackedSettingsWindow::ApplySearchFilter()
{
  if (!m_navigation_list || !m_stacked_panes)
    return;

  QString search_term = m_search_bar ? m_search_bar->text().trimmed() : QString{};
  if (search_term.size() < MIN_SETTINGS_SEARCH_LENGTH)
    search_term.clear();

  QPointer<QWidget> previous_current_match = nullptr;
  if (m_current_search_result_index >= 0 &&
      m_current_search_result_index < static_cast<int>(m_search_results.size()))
  {
    previous_current_match = m_search_results[m_current_search_result_index].widget;
  }

  m_search_results.clear();
  m_current_search_result_index = -1;

  int first_visible_row = -1;

  for (int i = 0; i < m_navigation_list->count(); ++i)
  {
    int match_count = 0;
    std::vector<QWidget*> pane_matches;
    QWidget* const pane = m_stacked_panes->widget(i);
    const bool pane_name_matches = i < static_cast<int>(m_pane_names.size()) &&
                                   m_pane_names[i].contains(search_term, Qt::CaseInsensitive);

    if (search_term.isEmpty())
    {
      if (i < static_cast<int>(m_first_match_widgets.size()))
        m_first_match_widgets[i] = nullptr;
    }
    else if (pane)
    {
      CollectMatchingWidgets(pane, search_term, &pane_matches);
      match_count = static_cast<int>(pane_matches.size());

      if (i < static_cast<int>(m_first_match_widgets.size()))
      {
        if (!pane_matches.empty())
          m_first_match_widgets[i] = pane_matches.front();
        else if (pane_name_matches)
          m_first_match_widgets[i] = pane;
        else
          m_first_match_widgets[i] = nullptr;
      }

      for (QWidget* const widget : pane_matches)
        m_search_results.emplace_back(SearchResult{i, QPointer<QWidget>(widget)});

      if (pane_name_matches && pane_matches.empty())
        m_search_results.emplace_back(SearchResult{i, QPointer<QWidget>(pane)});
    }

    const bool matches = search_term.isEmpty() || pane_name_matches || match_count > 0;

    QListWidgetItem* const item = m_navigation_list->item(i);
    item->setHidden(!matches);
    const QString pane_name =
        i < static_cast<int>(m_pane_names.size()) ? m_pane_names[i] : QString{};
    if (search_term.isEmpty() || match_count == 0)
      item->setText(QStringLiteral("  %1  ").arg(pane_name));
    else
      item->setText(QStringLiteral("  %1 (%2)  ").arg(pane_name).arg(match_count));

    if (matches && first_visible_row == -1)
      first_visible_row = i;
  }

  if (!search_term.isEmpty() && !m_search_results.empty())
  {
    if (previous_current_match)
    {
      const auto it = std::find_if(m_search_results.cbegin(), m_search_results.cend(),
                                   [&previous_current_match](const SearchResult& result) {
                                     return result.widget == previous_current_match;
                                   });
      if (it != m_search_results.cend())
      {
        m_current_search_result_index =
            static_cast<int>(std::distance(m_search_results.cbegin(), it));
      }
    }

    if (m_current_search_result_index < 0)
      m_current_search_result_index = 0;
  }

  ClearSearchHighlights();
  UpdateSearchResultControls();

  if (first_visible_row == -1)
  {
    m_stacked_panes->setEnabled(false);
    m_navigation_list->clearSelection();
    return;
  }

  m_stacked_panes->setEnabled(true);

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

  if (target_row != current_row)
    ActivatePane(target_row);

  UpdateCurrentPaneHighlights();
}

void StackedSettingsWindow::UpdateCurrentPaneHighlights()
{
  ClearSearchHighlights();

  if (!m_navigation_list || !m_stacked_panes || !m_search_bar)
    return;

  QString search_term = m_search_bar->text().trimmed();
  if (search_term.size() < MIN_SETTINGS_SEARCH_LENGTH)
    search_term.clear();
  if (search_term.isEmpty())
  {
    UpdateSearchResultControls();
    return;
  }

  const int current_row = m_navigation_list->currentRow();
  if (current_row < 0 || current_row >= m_stacked_panes->count())
  {
    UpdateSearchResultControls();
    return;
  }

  if (m_navigation_list->item(current_row)->isHidden())
  {
    UpdateSearchResultControls();
    return;
  }

  QWidget* const pane = m_stacked_panes->widget(current_row);
  if (!pane)
  {
    UpdateSearchResultControls();
    return;
  }

  std::vector<QWidget*> matches;
  CollectMatchingWidgets(pane, search_term, &matches);

  QWidget* current_match = nullptr;
  if (m_current_search_result_index >= 0 &&
      m_current_search_result_index < static_cast<int>(m_search_results.size()))
  {
    const SearchResult& result = m_search_results[m_current_search_result_index];
    if (result.pane_index == current_row)
      current_match = result.widget;
  }

  if (!current_match && !matches.empty())
  {
    current_match = matches.front();
    const auto current_it =
        std::find_if(m_search_results.cbegin(), m_search_results.cend(),
                     [current_row, current_match](const SearchResult& result) {
                       return result.pane_index == current_row && result.widget == current_match;
                     });
    if (current_it != m_search_results.cend())
    {
      m_current_search_result_index =
          static_cast<int>(std::distance(m_search_results.cbegin(), current_it));
    }
  }

  for (QWidget* const widget : matches)
  {
    widget->setProperty("settingsSearchMatch", true);
    widget->setProperty("settingsSearchCurrent", widget == current_match);
    widget->style()->unpolish(widget);
    widget->style()->polish(widget);
    widget->update();
    m_highlighted_widgets.emplace_back(widget);
  }

  QWidget* target_match = current_match;
  if (!target_match && current_row < static_cast<int>(m_first_match_widgets.size()))
    target_match = m_first_match_widgets[current_row];
  if (!target_match && !matches.empty())
    target_match = matches.front();

  EnsureWidgetIsVisible(target_match);
  UpdateSearchResultControls();
}

void StackedSettingsWindow::ClearSearchHighlights()
{
  for (const QPointer<QWidget>& widget : m_highlighted_widgets)
  {
    if (!widget)
      continue;

    widget->setProperty("settingsSearchMatch", false);
    widget->setProperty("settingsSearchCurrent", false);
    widget->style()->unpolish(widget);
    widget->style()->polish(widget);
    widget->update();
  }
  m_highlighted_widgets.clear();
}

void StackedSettingsWindow::UpdateSearchResultControls()
{
  if (!m_search_previous_button || !m_search_next_button || !m_search_results_label ||
      !m_search_navigation_widget)
  {
    return;
  }

  QString search_term = m_search_bar ? m_search_bar->text().trimmed() : QString{};
  const bool search_is_active = search_term.size() >= MIN_SETTINGS_SEARCH_LENGTH;
  const int result_count = search_is_active ? static_cast<int>(m_search_results.size()) : 0;
  const bool has_results = result_count > 0;

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

void StackedSettingsWindow::NavigateSearchResults(int direction)
{
  if (direction == 0 || !m_navigation_list || m_search_results.empty())
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
      ActivatePane(result.pane_index);
    else
      UpdateCurrentPaneHighlights();
    return;
  }
}

SettingsWindow::SettingsWindow(MainWindow* parent) : StackedSettingsWindow{parent}
{
  setWindowTitle(tr("Settings"));

  // If you change the order, don't forget to update the SettingsWindowPaneIndex enum.
  AddWrappedPane(new GeneralPane, tr("General"));
  AddPane(new GraphicsPane{parent, nullptr}, tr("Graphics"));
  AddWrappedPane(new ControllersPane, tr("Controllers"));
  AddWrappedPane(new InterfacePane, tr("Interface"));
  AddWrappedPane(new OnScreenDisplayPane, tr("On-Screen Display"));
  AddWrappedPane(new AudioPane, tr("Audio"));
  AddWrappedPane(new PathPane, tr("Paths"));
  AddWrappedPane(new GameCubePane{parent}, tr("GameCube"));
  AddWrappedPane(new WiiPane, tr("Wii"));
  AddWrappedPane(new TriforcePane, tr("Triforce"));
  AddWrappedPane(new AdvancedPane, tr("Advanced"));

  OnDoneCreatingPanes();
}

void SettingsWindow::SelectPane(SettingsWindowPaneIndex index)
{
  ActivatePane(std::to_underlying(index));
}
