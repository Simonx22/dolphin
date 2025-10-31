// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.utils

import android.app.Activity
import android.app.Application
import android.app.Application.ActivityLifecycleCallbacks
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import androidx.appcompat.app.AppCompatActivity
import androidx.appcompat.app.AppCompatDelegate
import androidx.preference.PreferenceManager
import org.dolphinemu.dolphinemu.features.settings.model.BooleanSetting
import org.dolphinemu.dolphinemu.features.settings.model.ConfigChangedCallback
import org.dolphinemu.dolphinemu.features.settings.model.IntSetting
import java.util.LinkedHashSet
import java.util.WeakHashMap

/**
 * Observes Dolphin's config for theme-related changes and keeps Android's theme state in sync.
 *
 * ThemeHelper relies on shared preferences to determine which theme should be applied before
 * Activity.onCreate runs. This watcher listens for native settings changes, updates those shared
 * preferences once per change, and recreates any currently resumed activities so they pick up the
 * new theme with minimal duplication across the UI layer.
 */
object ThemeSettingsWatcher : ActivityLifecycleCallbacks {
    private lateinit var application: Application
    private val handler = Handler(Looper.getMainLooper())
    private val resumedActivities = LinkedHashSet<AppCompatActivity>()
    private val activityGenerations = WeakHashMap<AppCompatActivity, Int>()
    private var configCallback: ConfigChangedCallback? = null

    @Volatile
    private var refreshScheduled = false
    @Volatile
    private var initialized = false

    private var lastTheme = ThemeHelper.DEFAULT
    private var lastThemeMode = AppCompatDelegate.MODE_NIGHT_FOLLOW_SYSTEM
    private var lastUseBlackBackgrounds = false
    private var themeChangeGeneration = 0

    fun initialize(application: Application) {
        if (initialized) return

        this.application = application
        application.registerActivityLifecycleCallbacks(this)

        // Capture the current values so we can detect changes going forward.
        lastTheme = IntSetting.MAIN_INTERFACE_THEME.int
        lastThemeMode = IntSetting.MAIN_INTERFACE_THEME_MODE.int
        lastUseBlackBackgrounds = BooleanSetting.MAIN_USE_BLACK_BACKGROUNDS.boolean
        syncPreferences(lastTheme, lastThemeMode, lastUseBlackBackgrounds)

        initialized = true
        configCallback = ConfigChangedCallback(Runnable { scheduleThemeRefresh() })
    }

    @JvmStatic
    fun ensureSynced() {
        if (!initialized) return
        scheduleThemeRefresh()
    }

    private fun scheduleThemeRefresh() {
        if (refreshScheduled) return

        refreshScheduled = true
        handler.post {
            refreshScheduled = false
            handleThemeSettingChange()
        }
    }

    private fun handleThemeSettingChange() {
        val theme = IntSetting.MAIN_INTERFACE_THEME.int
        val themeMode = IntSetting.MAIN_INTERFACE_THEME_MODE.int
        val useBlackBackgrounds = BooleanSetting.MAIN_USE_BLACK_BACKGROUNDS.boolean

        val themeChanged = theme != lastTheme
        val modeChanged = themeMode != lastThemeMode
        val backgroundChanged = useBlackBackgrounds != lastUseBlackBackgrounds

        syncPreferences(theme, themeMode, useBlackBackgrounds)

        if (!themeChanged && !modeChanged && !backgroundChanged) {
            return
        }

        lastTheme = theme
        lastThemeMode = themeMode
        lastUseBlackBackgrounds = useBlackBackgrounds
        themeChangeGeneration++

        recreateResumedActivities()
    }

    private fun syncPreferences(theme: Int, themeMode: Int, useBlackBackgrounds: Boolean) {
        val preferences = PreferenceManager.getDefaultSharedPreferences(application.applicationContext)
        val editor = preferences.edit()

        var modified = false
        if (preferences.getInt(ThemeHelper.CURRENT_THEME, ThemeHelper.DEFAULT) != theme) {
            editor.putInt(ThemeHelper.CURRENT_THEME, theme)
            modified = true
        }
        if (
            preferences.getInt(
                ThemeHelper.CURRENT_THEME_MODE,
                AppCompatDelegate.MODE_NIGHT_FOLLOW_SYSTEM
            ) != themeMode
        ) {
            editor.putInt(ThemeHelper.CURRENT_THEME_MODE, themeMode)
            modified = true
        }
        if (preferences.getBoolean(ThemeHelper.USE_BLACK_BACKGROUNDS, false) != useBlackBackgrounds) {
            editor.putBoolean(ThemeHelper.USE_BLACK_BACKGROUNDS, useBlackBackgrounds)
            modified = true
        }

        if (modified) {
            editor.apply()
        }
    }

    private fun recreateResumedActivities() {
        if (resumedActivities.isEmpty()) {
            return
        }

        for (activity in resumedActivities.toList()) {
            activityGenerations[activity] = themeChangeGeneration
            if (!activity.isFinishing && !activity.isDestroyed) {
                activity.recreate()
            }
        }
    }

    override fun onActivityResumed(activity: Activity) {
        if (activity is AppCompatActivity) {
            resumedActivities.add(activity)
            val handledGeneration = activityGenerations[activity]
            if (themeChangeGeneration != 0 && handledGeneration != themeChangeGeneration) {
                activityGenerations[activity] = themeChangeGeneration
                activity.window.decorView.post {
                    if (!activity.isFinishing && !activity.isDestroyed) {
                        activity.recreate()
                    }
                }
            }
        }
    }

    override fun onActivityPaused(activity: Activity) {
        if (activity is AppCompatActivity) {
            resumedActivities.remove(activity)
        }
    }

    override fun onActivityDestroyed(activity: Activity) {
        if (activity is AppCompatActivity) {
            resumedActivities.remove(activity)
            activityGenerations.remove(activity)
        }
    }

    override fun onActivityCreated(activity: Activity, savedInstanceState: Bundle?) {
        if (activity is AppCompatActivity) {
            activityGenerations[activity] = themeChangeGeneration
        }
    }

    override fun onActivityStarted(activity: Activity) {}

    override fun onActivityStopped(activity: Activity) {}

    override fun onActivitySaveInstanceState(activity: Activity, outState: Bundle) {}
}
