// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.utils

import androidx.fragment.app.FragmentActivity
import org.dolphinemu.dolphinemu.dialogs.AnalyticsDialog

object AnalyticsPromptCoordinator {
    @Volatile
    private var pending = false

    fun markPending() {
        pending = true
    }

    fun maybeShow(activity: FragmentActivity) {
        if (!pending) return
        if (activity.isFinishing || activity.isDestroyed) return
        if (activity.supportFragmentManager.findFragmentByTag(AnalyticsDialog.TAG) != null) return
        pending = false
        AnalyticsDialog().show(activity.supportFragmentManager, AnalyticsDialog.TAG)
    }
}
