// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.utils

import android.content.Context
import android.os.Handler
import android.os.Looper
import androidx.appcompat.app.AppCompatActivity
import androidx.fragment.app.FragmentActivity
import org.dolphinemu.dolphinemu.DolphinApplication
import org.dolphinemu.dolphinemu.features.settings.model.BooleanSetting
import org.dolphinemu.dolphinemu.features.settings.model.ConfigChangedCallback
import org.dolphinemu.dolphinemu.features.settings.model.IntSetting
import org.dolphinemu.dolphinemu.ui.main.ThemeProvider

/**
 * Observes Dolphin config changes related to themes and updates Android theming automatically.
 */
object ThemeConfigWatcher {
    private val handler = Handler(Looper.getMainLooper())
    private var callback: ConfigChangedCallback? = null

    private var lastTheme = IntSetting.MAIN_INTERFACE_THEME.int
    private var lastThemeMode = IntSetting.MAIN_INTERFACE_THEME_MODE.int
    private var lastBlackBackgrounds = BooleanSetting.MAIN_USE_BLACK_BACKGROUNDS.boolean

    fun initialize(context: Context) {
        if (callback != null) return

        val appContext = context.applicationContext
        ThemeHelper.updateThemePreferences(
            appContext,
            lastTheme,
            lastThemeMode,
            lastBlackBackgrounds
        )

        callback = ConfigChangedCallback(Runnable {
            handler.post { handleConfigChanged(appContext) }
        })
    }

    private fun handleConfigChanged(context: Context) {
        val newTheme = IntSetting.MAIN_INTERFACE_THEME.int
        val newThemeMode = IntSetting.MAIN_INTERFACE_THEME_MODE.int
        val newBlack = BooleanSetting.MAIN_USE_BLACK_BACKGROUNDS.boolean

        val themeChanged = newTheme != lastTheme
        val modeChanged = newThemeMode != lastThemeMode
        val blackChanged = newBlack != lastBlackBackgrounds

        val currentActivity = DolphinApplication.getAppActivity()

        if (!themeChanged && !modeChanged && !blackChanged) {
            if (currentActivity is FragmentActivity) {
                AnalyticsPromptCoordinator.maybeShow(currentActivity)
            }
            return
        }

        lastTheme = newTheme
        lastThemeMode = newThemeMode
        lastBlackBackgrounds = newBlack

        ThemeHelper.updateThemePreferences(context, newTheme, newThemeMode, newBlack)

        if (currentActivity is AppCompatActivity && currentActivity is ThemeProvider) {
            when {
                themeChanged || blackChanged -> currentActivity.recreate()
                modeChanged -> ThemeHelper.applyThemeMode(currentActivity)
            }
        }

        if (currentActivity is FragmentActivity) {
            AnalyticsPromptCoordinator.maybeShow(currentActivity)
        }
    }
}
