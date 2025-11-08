// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.dialogs

import android.app.Dialog
import android.os.Bundle
import android.widget.Toast
import androidx.fragment.app.DialogFragment
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import org.dolphinemu.dolphinemu.DolphinApplication
import org.dolphinemu.dolphinemu.R
import org.dolphinemu.dolphinemu.activities.ConvertActivity
import org.dolphinemu.dolphinemu.features.cheats.ui.CheatsActivity
import org.dolphinemu.dolphinemu.features.riivolution.ui.RiivolutionBootActivity
import org.dolphinemu.dolphinemu.features.settings.model.Settings
import org.dolphinemu.dolphinemu.features.settings.model.StringSetting
import org.dolphinemu.dolphinemu.features.settings.ui.MenuTag
import org.dolphinemu.dolphinemu.features.settings.ui.SettingsActivity
import org.dolphinemu.dolphinemu.model.GameFile
import org.dolphinemu.dolphinemu.ui.main.MainPresenter
import org.dolphinemu.dolphinemu.ui.platform.Platform
import org.dolphinemu.dolphinemu.utils.AlertDialogItemsBuilder
import org.dolphinemu.dolphinemu.utils.DirectoryInitialization
import org.dolphinemu.dolphinemu.utils.Log
import java.io.File

class GamePropertiesDialog : DialogFragment() {
  override fun onCreateDialog(savedInstanceState: Bundle?): Dialog {
    val args = requireArguments()

    val path = requireNotNull(args.getString(ARG_PATH))
        val gameId = requireNotNull(args.getString(ARG_GAME_ID))
        val gameTdbId: String? = args.getString(ARG_GAMETDB_ID)
    val revision = args.getInt(ARG_REVISION)
    val discNumber = args.getInt(ARG_DISC_NUMBER)
    val platform = args.getInt(ARG_PLATFORM)
    val shouldAllowConversion = args.getBoolean(ARG_SHOULD_ALLOW_CONVERSION)

    val isDisc = platform == Platform.GAMECUBE.toInt() ||
      platform == Platform.TRIFORCE.toInt() || platform == Platform.WII.toInt()
    val isWii = platform == Platform.WII.toInt() ||
      platform == Platform.WIIWARE.toInt()

    val itemsBuilder = AlertDialogItemsBuilder(requireContext())

    itemsBuilder.add(R.string.properties_details) { _, _ ->
      GameDetailsDialog.newInstance(path)
        .show(requireActivity().supportFragmentManager, "game_details")
    }

    if (isDisc) {
            itemsBuilder.add(R.string.properties_start_with_riivolution) { _, _ ->
                RiivolutionBootActivity.launch(requireContext(), path, gameId, revision, discNumber)
            }

            itemsBuilder.add(R.string.properties_set_default_iso) { _, _ ->
                val settings = Settings()
                try {
                    settings.loadSettings()
                    StringSetting.MAIN_DEFAULT_ISO.setString(settings, path)
                    settings.saveSettings()
                } finally {
                    settings.close()
                }
            }
    }

    if (shouldAllowConversion) {
      itemsBuilder.add(R.string.properties_convert) { _, _ ->
        ConvertActivity.launch(requireContext(), path)
      }
    }

    if (isDisc && isWii) {
      itemsBuilder.add(R.string.properties_system_update) { _, _ ->
        MainPresenter.launchDiscUpdate(path, requireActivity())
      }
    }

    itemsBuilder.add(R.string.properties_edit_game_settings) { _, _ ->
      SettingsActivity.launch(requireContext(), MenuTag.SETTINGS, gameId, revision, isWii)
    }

            itemsBuilder.add(R.string.properties_edit_cheats) { _, _ ->
                CheatsActivity.launch(requireContext(), gameId, gameTdbId, revision, isWii)
            }

    itemsBuilder.add(R.string.properties_clear_game_settings) { _, _ ->
      clearGameSettingsWithConfirmation(gameId)
    }

    val builder = MaterialAlertDialogBuilder(requireContext())
      .setTitle(
        requireContext().getString(
          R.string.preferences_game_properties_with_game_id,
          gameId
        )
      )
    itemsBuilder.applyToBuilder(builder)
    return builder.create()
  }

  private fun clearGameSettingsWithConfirmation(gameId: String) {
    MaterialAlertDialogBuilder(requireContext())
      .setTitle(R.string.properties_clear_game_settings)
      .setMessage(R.string.properties_clear_game_settings_confirmation)
      .setPositiveButton(R.string.yes) { _, _ -> clearGameSettings(gameId) }
      .setNegativeButton(R.string.no, null)
      .show()
  }

  private fun clearGameSettings(gameId: String) {
    val context = DolphinApplication.getAppContext()
    val gameSettingsPath =
      "${DirectoryInitialization.getUserDirectory()}/GameSettings/$gameId.ini"
    val gameProfilesPath = "${DirectoryInitialization.getUserDirectory()}/Config/Profiles/"
    val gameSettingsFile = File(gameSettingsPath)
    val gameProfilesDirectory = File(gameProfilesPath)
    val hadGameProfiles = recursivelyDeleteGameProfiles(gameProfilesDirectory, gameId)

    if (gameSettingsFile.exists() || hadGameProfiles) {
      if (gameSettingsFile.delete() || hadGameProfiles) {
        Toast.makeText(
          context,
          context.getString(R.string.properties_clear_success, gameId),
          Toast.LENGTH_SHORT
        ).show()
      } else {
        Toast.makeText(
          context,
          context.getString(R.string.properties_clear_failure, gameId),
          Toast.LENGTH_SHORT
        ).show()
      }
    } else {
      Toast.makeText(context, R.string.properties_clear_missing, Toast.LENGTH_SHORT).show()
    }
  }

  private fun recursivelyDeleteGameProfiles(file: File, gameId: String): Boolean {
    var hadGameProfiles = false

    if (file.isDirectory) {
      val files = file.listFiles() ?: return false

            for (child in files) {
                if (child.name.startsWith(gameId) && child.isFile) {
                    if (!child.delete()) {
                        Log.error("[GamePropertiesDialog] Failed to delete ${child.absolutePath}")
                    }
                    hadGameProfiles = true
                }
                hadGameProfiles = recursivelyDeleteGameProfiles(child, gameId) || hadGameProfiles
            }
        }

    return hadGameProfiles
  }

  companion object {
    const val TAG = "GamePropertiesDialog"
    private const val ARG_PATH = "path"
    private const val ARG_GAME_ID = "game_id"
    private const val ARG_GAMETDB_ID = "gametdb_id"
    const val ARG_REVISION = "revision"
    const val ARG_DISC_NUMBER = "disc_number"
    private const val ARG_PLATFORM = "platform"
    private const val ARG_SHOULD_ALLOW_CONVERSION = "should_allow_conversion"

    fun newInstance(gameFile: GameFile): GamePropertiesDialog {
      val fragment = GamePropertiesDialog()

      val arguments = Bundle()
      arguments.putString(ARG_PATH, gameFile.getPath())
      arguments.putString(ARG_GAME_ID, gameFile.getGameId())
      arguments.putString(ARG_GAMETDB_ID, gameFile.getGameTdbId())
      arguments.putInt(ARG_REVISION, gameFile.getRevision())
      arguments.putInt(ARG_DISC_NUMBER, gameFile.getDiscNumber())
      arguments.putInt(ARG_PLATFORM, gameFile.getPlatform())
      arguments.putBoolean(ARG_SHOULD_ALLOW_CONVERSION, gameFile.shouldAllowConversion())
      fragment.arguments = arguments

      return fragment
    }
  }
}
