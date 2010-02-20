#include <sys/utsname.h>
#include <glib/gi18n-lib.h>
#include <alpm.h>
#include "pacman-error.h"
#include "pacman-list.h"
#include "pacman-database.h"
#include "pacman-transaction.h"
#include "pacman-manager.h"

/**
 * SECTION:pacman-manager
 * @title: PacmanManager
 * @short_description: Package management
 *
 * A #PacmanManager object manages settings, transactions and other important operations. A reference to the global #PacmanManager object can be obtained using pacman_manager_get(), and package management can be configured from a config file using pacman_manager_configure(). Alternatively, #PacmanManager supplies methods with which it can be configured manually. Transactions can be created using pacman_manager_install(), pacman_manager_remove(), pacman_manager_sync() and pacman_manager_update().
 */

/**
 * PacmanManager:
 *
 * Represents a reference to the package management system.
 */

#define PACMAN_MANAGER_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), PACMAN_TYPE_MANAGER, PacmanManagerPrivate))

G_DEFINE_TYPE (PacmanManager, pacman_manager, G_TYPE_OBJECT);

PacmanManager *pacman_manager = NULL;

typedef struct _PacmanManagerPrivate {
	PacmanTransaction *transaction;
	
	gboolean i_love_candy;
	gboolean show_size;
	gboolean total_download;
	gboolean use_delta;
	
	gchar *clean_method;
	GClosure *transfer;
	
	PacmanList *hold_packages;
	PacmanList *sync_firsts;
} PacmanManagerPrivate;

static PacmanManager *pacman_manager_new (void) {
	return (PacmanManager *) g_object_new (PACMAN_TYPE_MANAGER, NULL);
}

static void pacman_manager_init (PacmanManager *manager) {
	PacmanManagerPrivate *priv;
	
	g_return_if_fail (manager != NULL);
	
	priv = PACMAN_MANAGER_GET_PRIVATE (manager);
	priv->clean_method = g_strdup ("KeepInstalled");
}

static void pacman_manager_finalize (GObject *object) {
	PacmanManagerPrivate *priv;
	
	g_return_if_fail (object != NULL);
	
	priv = PACMAN_MANAGER_GET_PRIVATE (PACMAN_MANAGER (object));
	if (priv->transaction != NULL) {
		g_object_unref (priv->transaction);
	}
	
	g_free (priv->clean_method);
	if (priv->transfer != NULL) {
		g_closure_unref (priv->transfer);
	}
	
	pacman_list_free_full (priv->hold_packages, g_free);
	pacman_list_free_full (priv->sync_firsts, g_free);
	
	pacman_manager = NULL;
	alpm_release ();
	
	G_OBJECT_CLASS (pacman_manager_parent_class)->finalize (object);
}

static void pacman_manager_log_cb (pmloglevel_t level, gchar *format, va_list args) {
	GLogLevelFlags flags = 0;
	
	if (format != NULL && format[0] != '\0') {
		return;
	}
	
	switch (level) {
		case PM_LOG_DEBUG:
			flags |= G_LOG_LEVEL_DEBUG;
			break;
		
		case PM_LOG_ERROR:
			flags |= G_LOG_LEVEL_CRITICAL;
			break;
		
		case PM_LOG_WARNING:
			flags |= G_LOG_LEVEL_WARNING;
			break;
		
		case PM_LOG_FUNCTION:
			flags |= G_LOG_LEVEL_INFO;
			break;
	}
	
	g_logv (G_LOG_DOMAIN, flags, format, args);
}

static gboolean pacman_set_user_agent (void) {
	gchar *user_agent;
	struct utsname un;
	
	uname (&un);
	user_agent = g_strdup_printf ("%s/%s (%s %s) libalpm/%s", PACKAGE_TARNAME, PACKAGE_VERSION, un.sysname, un.machine, alpm_version ());
	g_setenv ("HTTP_USER_AGENT", user_agent, FALSE);
	g_free (user_agent);
}

/**
 * pacman_manager_get:
 * @error: A #GError, or %NULL
 *
 * Gets a reference to the global #PacmanManager object.
 *
 * Returns: A #PacmanManager, or %NULL if @error is set. Free with g_object_unref().
 */
PacmanManager *pacman_manager_get (GError **error) {
	if (pacman_manager == NULL) {
		if (alpm_initialize () < 0) {
			g_set_error (error, PACMAN_ERROR, pm_errno, _("Failed to initialize alpm: %s"), alpm_strerrorlast ());
			return NULL;
		}
		
		alpm_option_set_logcb (pacman_manager_log_cb);
		alpm_option_set_root (PACMAN_ROOT_PATH);
		alpm_option_set_dbpath (PACMAN_DATABASE_PATH);
		alpm_option_set_logfile (PACMAN_LOG_FILE);
		
		pacman_set_user_agent ();
		pacman_manager = pacman_manager_new ();
	} else {
		g_object_ref (pacman_manager);
	}
	
	return pacman_manager;
}

/**
 * pacman_manager_get_root_path:
 * @manager: A #PacmanManager.
 *
 * Gets the directory that pacman will use as the root filesystem.
 *
 * Returns: A directory name. Do not free.
 */
const gchar *pacman_manager_get_root_path (PacmanManager *manager) {
	g_return_val_if_fail (manager != NULL, NULL);
	
	return alpm_option_get_root ();
}

/**
 * pacman_manager_set_root_path:
 * @manager: A #PacmanManager.
 * @path: A directory name.
 * @error: A #GError, or %NULL.
 *
 * Sets the RootDir to @path. See pacman_manager_get_root_path().
 *
 * Returns: %TRUE if the RootDir was set successfully, or %FALSE if @error is set.
 */
gboolean pacman_manager_set_root_path (PacmanManager *manager, const gchar *path, GError **error) {
	g_return_val_if_fail (manager != NULL, FALSE);
	g_return_val_if_fail (path != NULL, FALSE);
	
	if (alpm_option_set_root (path) < 0) {
		g_set_error (error, PACMAN_ERROR, pm_errno, _("Could not set root directory: %s"), alpm_strerrorlast ());
		return FALSE;
	}
	
	return TRUE;
}

/**
 * pacman_manager_get_database_path:
 * @manager: A #PacmanManager.
 *
 * Gets the directory where package databases are stored.
 *
 * Returns: A directory name. Do not free.
 */
const gchar *pacman_manager_get_database_path (PacmanManager *manager) {
	g_return_val_if_fail (manager != NULL, NULL);
	
	return alpm_option_get_dbpath ();
}

/**
 * pacman_manager_set_database_path:
 * @manager: A #PacmanManager.
 * @path: A directory name.
 * @error: A #GError, or %NULL.
 *
 * Sets the DBPath to @path. See pacman_manager_get_database_path().
 *
 * Returns: %TRUE if the DBPath was set successfully, or %FALSE if @error is set.
 */
gboolean pacman_manager_set_database_path (PacmanManager *manager, const gchar *path, GError **error) {
	g_return_val_if_fail (manager != NULL, FALSE);
	g_return_val_if_fail (path != NULL, FALSE);
	
	if (alpm_option_set_dbpath (path) < 0) {
		g_set_error (error, PACMAN_ERROR, pm_errno, _("Could not set database directory: %s"), alpm_strerrorlast ());
		return FALSE;
	}
	
	return TRUE;
}

/**
 * pacman_manager_get_log_file:
 * @manager: A #PacmanManager.
 *
 * Gets the file that pacman will use for logging.
 *
 * Returns: A file name. Do not free.
 */
const gchar *pacman_manager_get_log_file (PacmanManager *manager) {
	g_return_val_if_fail (manager != NULL, NULL);
	
	return alpm_option_get_logfile ();
}

/**
 * pacman_manager_set_log_file:
 * @manager: A #PacmanManager.
 * @filename: A file name.
 *
 * Sets the LogFile to @filename. See pacman_manager_get_log_file().
 */
void pacman_manager_set_log_file (PacmanManager *manager, const gchar *filename) {
	g_return_if_fail (manager != NULL);
	g_return_if_fail (filename != NULL);
	
	alpm_option_set_logfile (filename);
}

/**
 * pacman_manager_get_cache_paths:
 * @manager: A #PacmanManager.
 *
 * Gets a list of directories that pacman will use to store downloaded packages.
 *
 * Returns: A list of directory names. Do not free.
 */
const PacmanList *pacman_manager_get_cache_paths (PacmanManager *manager) {
	g_return_val_if_fail (manager != NULL, NULL);
	
	return alpm_option_get_cachedirs ();
}

/**
 * pacman_manager_add_cache_path:
 * @manager: A #PacmanManager.
 * @path: A directory name.
 *
 * Adds @path to the list of CacheDirs. See pacman_manager_get_cache_paths().
 */
void pacman_manager_add_cache_path (PacmanManager *manager, const gchar *path) {
	g_return_if_fail (manager != NULL);
	g_return_if_fail (path != NULL);
	
	alpm_option_add_cachedir (path);
}

/**
 * pacman_manager_remove_cache_path:
 * @manager: A #PacmanManager.
 * @path: A directory name.
 *
 * Removes @path from the list of CacheDirs. See pacman_manager_get_cache_paths().
 *
 * Returns: %TRUE if @path was removed, or %FALSE if it was not found.
 */
gboolean pacman_manager_remove_cache_path (PacmanManager *manager, const gchar *path) {
	g_return_val_if_fail (manager != NULL, FALSE);
	g_return_val_if_fail (path != NULL, FALSE);
	
	return alpm_option_remove_cachedir (path);
}

/**
 * pacman_manager_set_cache_paths:
 * @manager: A #PacmanManager.
 * @cache_paths: A list of directory names.
 *
 * Sets the list of CacheDirs to @cache_paths. See pacman_manager_get_cache_paths().
 */
void pacman_manager_set_cache_paths (PacmanManager *manager, PacmanList *cache_paths) {
	g_return_if_fail (manager != NULL);
	g_return_if_fail (cache_paths != NULL);
	
	alpm_option_set_cachedirs (alpm_list_strdup (cache_paths));
}

/**
 * pacman_manager_get_i_love_candy:
 * @manager: A #PacmanManager.
 *
 * Checks whether or not you love candy ;).
 *
 * Returns: %TRUE if you love candy, %FALSE otherwise.
 */
gboolean pacman_manager_get_i_love_candy (PacmanManager *manager) {
	PacmanManagerPrivate *priv;
	
	g_return_val_if_fail (manager != NULL, FALSE);
	
	priv = PACMAN_MANAGER_GET_PRIVATE (manager);
	return priv->i_love_candy;
}

/**
 * pacman_manager_set_i_love_candy:
 * @manager: A #PacmanManager.
 * @value: %TRUE or %FALSE.
 *
 * Sets ILoveCandy to @value. See pacman_manager_get_i_love_candy().
 */
void pacman_manager_set_i_love_candy (PacmanManager *manager, gboolean value) {
	PacmanManagerPrivate *priv;
	
	g_return_if_fail (manager != NULL);
	
	priv = PACMAN_MANAGER_GET_PRIVATE (manager);
	priv->i_love_candy = value;
}

/**
 * pacman_manager_get_show_size:
 * @manager: A #PacmanManager.
 *
 * Checks whether package sizes should be displayed in search results.
 *
 * Returns: %TRUE if sizes should be displayed, %FALSE otherwise.
 */
gboolean pacman_manager_get_show_size (PacmanManager *manager) {
	PacmanManagerPrivate *priv;
	
	g_return_val_if_fail (manager != NULL, FALSE);
	
	priv = PACMAN_MANAGER_GET_PRIVATE (manager);
	return priv->show_size;
}

/**
 * pacman_manager_set_show_size:
 * @manager: A #PacmanManager.
 * @value: %TRUE or %FALSE.
 *
 * Sets ShowSize to @value. See pacman_manager_get_show_size().
 */
void pacman_manager_set_show_size (PacmanManager *manager, gboolean value) {
	PacmanManagerPrivate *priv;
	
	g_return_if_fail (manager != NULL);
	
	priv = PACMAN_MANAGER_GET_PRIVATE (manager);
	priv->show_size = value;
}

/**
 * pacman_manager_get_total_download:
 * @manager: A #PacmanManager.
 *
 * Checks whether the total progress of all downloads should be displayed.
 *
 * Returns: %TRUE if the total progress should be displayed, %FALSE otherwise.
 */
gboolean pacman_manager_get_total_download (PacmanManager *manager) {
	PacmanManagerPrivate *priv;
	
	g_return_val_if_fail (manager != NULL, FALSE);
	
	priv = PACMAN_MANAGER_GET_PRIVATE (manager);
	return priv->total_download;
}

/**
 * pacman_manager_set_total_download:
 * @manager: A #PacmanManager.
 * @value: %TRUE or %FALSE.
 *
 * Sets TotalDownload to @value. See pacman_manager_get_total_download().
 */
void pacman_manager_set_total_download (PacmanManager *manager, gboolean value) {
	PacmanManagerPrivate *priv;
	
	g_return_if_fail (manager != NULL);
	
	priv = PACMAN_MANAGER_GET_PRIVATE (manager);
	priv->total_download = value;
}

/**
 * pacman_manager_get_no_passive_ftp:
 * @manager: A #PacmanManager.
 *
 * Checks whether pacman will use FTP passive mode when downloading files.
 *
 * Returns: %TRUE if passive mode will be disabled, %FALSE otherwise.
 */
gboolean pacman_manager_get_no_passive_ftp (PacmanManager *manager) {
	g_return_val_if_fail (manager != NULL, FALSE);
	
	return alpm_option_get_nopassiveftp ();
}

/**
 * pacman_manager_set_no_passive_ftp:
 * @manager: A #PacmanManager.
 * @value: %TRUE or %FALSE.
 *
 * Sets NoPassiveFtp to @value. See pacman_manager_get_no_passive_ftp().
 */
void pacman_manager_set_no_passive_ftp (PacmanManager *manager, gboolean value) {
	g_return_if_fail (manager != NULL);
	
	alpm_option_set_nopassiveftp (value);
}

/**
 * pacman_manager_get_use_delta:
 * @manager: A #PacmanManager.
 *
 * Checks whether pacman will download delta patches instead of complete packages if available.
 *
 * Returns: %TRUE if delta patches will used, %FALSE otherwise.
 */
gboolean pacman_manager_get_use_delta (PacmanManager *manager) {
	PacmanManagerPrivate *priv;
	
	g_return_val_if_fail (manager != NULL, FALSE);
	
	priv = PACMAN_MANAGER_GET_PRIVATE (manager);
	return priv->use_delta;
}

/**
 * pacman_manager_set_use_delta:
 * @manager: A #PacmanManager.
 * @value: %TRUE or %FALSE.
 *
 * Sets UseDelta to @value. See pacman_manager_get_use_delta().
 */
void pacman_manager_set_use_delta (PacmanManager *manager, gboolean value) {
	PacmanManagerPrivate *priv;
	
	g_return_if_fail (manager != NULL);
	
	priv = PACMAN_MANAGER_GET_PRIVATE (manager);
	priv->use_delta = value;
	alpm_option_set_usedelta (value);
}

/**
 * pacman_manager_get_use_syslog:
 * @manager: A #PacmanManager.
 *
 * Checks whether pacman will use syslog for logging.
 *
 * Returns: %TRUE if syslog will be used, %FALSE otherwise.
 */
gboolean pacman_manager_get_use_syslog (PacmanManager *manager) {
	g_return_val_if_fail (manager != NULL, FALSE);
	
	return alpm_option_get_usesyslog ();
}

/**
 * pacman_manager_set_use_syslog:
 * @manager: A #PacmanManager.
 * @value: %TRUE or %FALSE.
 *
 * Sets UseSyslog to @value. See pacman_manager_get_use_syslog().
 */
void pacman_manager_set_use_syslog (PacmanManager *manager, gboolean value) {
	g_return_if_fail (manager != NULL);
	
	alpm_option_set_usesyslog (value);
}

static gint pacman_manager_fetch_cb (const gchar *url, const gchar *path, time_t old_mtime, time_t *new_mtime) {
	PacmanManagerPrivate *priv;
	GValue result = { 0 }, params[4] = { 0 };
	time_t mtime;
	
	g_return_val_if_fail (url != NULL, -1);
	g_return_val_if_fail (path != NULL, -1);
	g_return_val_if_fail (pacman_manager != NULL, -1);
	
	priv = PACMAN_MANAGER_GET_PRIVATE (pacman_manager);
	mtime = old_mtime;
	
	g_value_init (&params[0], PACMAN_TYPE_MANAGER);
	g_value_set_instance (&params[0], pacman_manager);
	
	g_value_init (&params[1], G_TYPE_STRING);
	g_value_set_string (&params[1], url);
	
	g_value_init (&params[2], G_TYPE_STRING);
	g_value_set_string (&params[2], path);
	
	g_value_init (&params[3], G_TYPE_POINTER);
	g_value_set_pointer (&params[3], &mtime);
	
	g_value_init (&result, G_TYPE_BOOLEAN);
	g_closure_invoke (priv->transfer, &result, 4, params, NULL);
	
	if (!g_value_get_boolean (&result)) {
		return -1;
	}
	
	if (new_mtime != NULL) {
		*new_mtime = mtime;
	}
	
	return (mtime == old_mtime) ? 1 : 0;
}

/**
 * pacman_manager_set_transfer_closure:
 * @manager: A #PacmanManager.
 * @transfer_closure: A #GClosure, or %NULL to download files internally.
 *
 * Sets the closure that pacman will use to download files to @transfer_closure. See #PacmanTransferFunc.
 */
void pacman_manager_set_transfer_closure (PacmanManager *manager, GClosure *transfer_closure) {
	PacmanManagerPrivate *priv;
	
	g_return_if_fail (manager != NULL);
	
	priv = PACMAN_MANAGER_GET_PRIVATE (manager);
	if (priv->transfer != NULL) {
		g_closure_unref (priv->transfer);
	}
	
	if (transfer_closure == NULL) {
		alpm_option_set_fetchcb (NULL);
		priv->transfer = NULL;
	} else {
		if (priv->transfer == NULL) {
			alpm_option_set_fetchcb (pacman_manager_fetch_cb);
		}
		priv->transfer = transfer_closure;
	}
}

void g_cclosure_user_marshal_BOOLEAN__STRING_STRING_POINTER (GClosure *closure, GValue *result, guint param_count, const GValue *param_values, gpointer hint, gpointer marshal_data);

/**
 * pacman_manager_set_transfer_handler:
 * @manager: A #PacmanManager.
 * @transfer_func: A #PacmanTransferFunc function, or %NULL to download files internally.
 * @user_data: User data to pass to @transfer_func.
 * @destroy_data: A #GClosureNotify to be called when @user_data is no longer needed.
 *
 * Sets the function that pacman will use to download files to @transfer_func. See #PacmanTransferFunc.
 */
void pacman_manager_set_transfer_handler (PacmanManager *manager, PacmanTransferFunc transfer_func, gpointer user_data, GClosureNotify destroy_data) {
	g_return_if_fail (manager != NULL);
	
	if (transfer_func == NULL) {
		pacman_manager_set_transfer_closure (manager, NULL);
	} else {
		GClosure *transfer_closure = g_cclosure_new (G_CALLBACK (transfer_func), user_data, destroy_data);
		g_closure_set_marshal (transfer_closure, g_cclosure_user_marshal_BOOLEAN__STRING_STRING_POINTER);
		pacman_manager_set_transfer_closure (manager, transfer_closure);
	}
}

static gboolean pacman_transfer_with_command (PacmanManager *manager, const gchar *url, const gchar *path, time_t *mtime, gpointer user_data) {
	const gchar *transfer_command = (const gchar *) user_data;
	GRegex *input, *output;
	gchar *old_pwd, *basename, *tempname, *filename;
	gchar *temp_command, *final_command;
	gint exit_status = -1;
	
	g_return_val_if_fail (url != NULL, FALSE);
	g_return_val_if_fail (path != NULL, FALSE);
	g_return_val_if_fail (transfer_command != NULL, FALSE);
	
	old_pwd = g_get_current_dir ();
	if (g_chdir (path) < 0) {
		g_warning ("Could not locate download directory %s\n", path);
		g_free (old_pwd);
		return FALSE;
	}
	
	input = g_regex_new ("%u", 0, 0, NULL);
	if (input == NULL) {
		g_chdir (old_pwd);
		g_free (old_pwd);
		return FALSE;
	}
	
	output = g_regex_new ("%o", 0, 0, NULL);
	if (output == NULL) {
		g_regex_unref (input);
		g_chdir (old_pwd);
		g_free (old_pwd);
		return FALSE;
	}
	
	basename = g_path_get_basename (url);
	tempname = g_strdup_printf ("%s%s.part", path, basename);
	filename = g_strdup_printf ("%s%s", path, basename);
	
	temp_command = g_regex_replace_literal (output, transfer_command, -1, 0, tempname, 0, NULL);
	if (temp_command != NULL) {
		if (strcmp (temp_command, transfer_command) == 0) {
			/* not using .part filename */
			g_free (tempname);
			tempname = NULL;
		}
		
		final_command = g_regex_replace_literal (input, temp_command, -1, 0, url, 0, NULL);
		if (final_command != NULL) {
			if (!g_spawn_command_line_sync (final_command, NULL, NULL, &exit_status, NULL)) {
				g_warning ("Could not run transfer command %s\n", final_command);
			} else if (exit_status != 0) {
				g_warning ("Transfer command returned error code %d\n", exit_status);
			} else if (tempname != NULL) {
				/* using .part filename */
				if (g_rename (tempname, filename) < 0) {
					g_warning ("Could not rename resulting file %s\n", tempname);
					exit_status = -1;
				}
			}
		}
	}
	
	/* temp_command might be garbage if fc is NULL */
	if (final_command != NULL) {
		g_free (final_command);
		g_free (temp_command);
	}
	
	g_free (filename);
	g_free (basename);
	g_free (tempname);
	
	g_regex_unref (output);
	g_regex_unref (input);
	
	g_chdir (old_pwd);
	g_free (old_pwd);
	
	return exit_status == 0;
}

static void pacman_transfer_command_free (gpointer user_data, GClosure *closure) {
	g_free (user_data);
}

/**
 * pacman_manager_set_transfer_command:
 * @manager: A #PacmanManager.
 * @command: A command, or %NULL to download files internally.
 *
 * Sets the command pacman will use to download files to @command. The placeholder \%u will be replaced by the URL of the file to download, and \%o will be replaced by a path where the file should be downloaded to.
 */
void pacman_manager_set_transfer_command (PacmanManager *manager, const gchar *command) {
	g_return_if_fail (manager != NULL);
	
	if (command != NULL) {
		pacman_manager_set_transfer_handler (manager, pacman_transfer_with_command, g_strdup (command), pacman_transfer_command_free);
	} else {
		pacman_manager_set_transfer_closure (manager, NULL);
	}
}

/**
 * pacman_manager_get_clean_method:
 * @manager: A #PacmanManager.
 *
 * Gets the method used to determine which packages will remain in the cache after cleaning it. This could be "KeepInstalled" to keep currently installed packages, or "KeepCurrent" to keep packages that are not outdated.
 *
 * Returns: Either "KeepInstalled" or "KeepCurrent". Do not free.
 */
const gchar *pacman_manager_get_clean_method (PacmanManager *manager) {
	PacmanManagerPrivate *priv;
	
	g_return_val_if_fail (manager != NULL, NULL);
	
	priv = PACMAN_MANAGER_GET_PRIVATE (manager);
	return priv->clean_method;
}

/**
 * pacman_manager_set_clean_method:
 * @manager: A #PacmanManager.
 * @method: A clean method.
 *
 * Sets the CleanMethod to @method. See pacman_manager_get_clean_method().
 */
void pacman_manager_set_clean_method (PacmanManager *manager, const gchar *method) {
	PacmanManagerPrivate *priv;
	
	g_return_if_fail (manager != NULL);
	g_return_if_fail (method != NULL);
	
	priv = PACMAN_MANAGER_GET_PRIVATE (manager);
	g_free (priv->clean_method);
	priv->clean_method = g_strdup (method);
}

/**
 * pacman_manager_get_hold_packages:
 * @manager: A #PacmanManager.
 *
 * Gets a list of packages that will not be removed without user confirmation.
 *
 * Returns: A list of package names. Do not free.
 */
const PacmanList *pacman_manager_get_hold_packages (PacmanManager *manager) {
	PacmanManagerPrivate *priv;
	
	g_return_val_if_fail (manager != NULL, NULL);
	
	priv = PACMAN_MANAGER_GET_PRIVATE (manager);
	return priv->hold_packages;
}

/**
 * pacman_manager_add_hold_package:
 * @manager: A #PacmanManager.
 * @package: The name of a package.
 *
 * Adds @package to the list of HoldPkgs. See pacman_manager_get_hold_packages().
 */
void pacman_manager_add_hold_package (PacmanManager *manager, const gchar *package) {
	PacmanManagerPrivate *priv;
	
	g_return_if_fail (manager != NULL);
	g_return_if_fail (package != NULL);
	
	priv = PACMAN_MANAGER_GET_PRIVATE (manager);
	priv->hold_packages = pacman_list_add (priv->hold_packages, g_strdup (package));
}

/**
 * pacman_manager_remove_hold_package:
 * @manager: A #PacmanManager.
 * @package: The name of a package.
 *
 * Removes @package from the list of HoldPkgs. See pacman_manager_get_hold_packages().
 *
 * Returns: %TRUE if @package was removed, or %FALSE if it was not found.
 */
gboolean pacman_manager_remove_hold_package (PacmanManager *manager, const gchar *package) {
	PacmanManagerPrivate *priv;
	gchar *removed;
	
	g_return_val_if_fail (manager != NULL, FALSE);
	g_return_val_if_fail (package != NULL, FALSE);
	
	priv = PACMAN_MANAGER_GET_PRIVATE (manager);
	priv->hold_packages = pacman_list_remove_string (priv->hold_packages, package, &removed);
	
	if (removed != NULL) {
		g_free (removed);
		return TRUE;
	} else {
		return FALSE;
	}
}

/**
 * pacman_manager_set_hold_packages:
 * @manager: A #PacmanManager.
 * @hold_packages: A list of package names.
 *
 * Sets the list of HoldPkgs to @hold_packages. See pacman_manager_get_hold_packages().
 */
void pacman_manager_set_hold_packages (PacmanManager *manager, PacmanList *hold_packages) {
	PacmanManagerPrivate *priv;
	
	g_return_if_fail (manager != NULL);
	
	priv = PACMAN_MANAGER_GET_PRIVATE (manager);
	pacman_list_free_full (priv->hold_packages, (GDestroyNotify) g_free);
	priv->hold_packages = pacman_list_strdup (hold_packages);
}

/**
 * pacman_manager_get_sync_firsts:
 * @manager: A #PacmanManager.
 *
 * Gets a list of packages that will be upgraded before any others.
 *
 * Returns: A list of package names. Do not free.
 */
const PacmanList *pacman_manager_get_sync_firsts (PacmanManager *manager) {
	PacmanManagerPrivate *priv;
	
	g_return_val_if_fail (manager != NULL, NULL);
	
	priv = PACMAN_MANAGER_GET_PRIVATE (manager);
	return priv->sync_firsts;
}

/**
 * pacman_manager_add_sync_first:
 * @manager: A #PacmanManager.
 * @package: The name of a package.
 *
 * Adds @package to the list of SyncFirsts. See pacman_manager_get_sync_firsts().
 */
void pacman_manager_add_sync_first (PacmanManager *manager, const gchar *package) {
	PacmanManagerPrivate *priv;
	
	g_return_if_fail (manager != NULL);
	g_return_if_fail (package != NULL);
	
	priv = PACMAN_MANAGER_GET_PRIVATE (manager);
	priv->sync_firsts = pacman_list_add (priv->sync_firsts, g_strdup (package));
}

/**
 * pacman_manager_remove_sync_first:
 * @manager: A #PacmanManager.
 * @package: The name of a package.
 *
 * Removes @package from the list of SyncFirsts. See pacman_manager_get_sync_firsts().
 *
 * Returns: %TRUE if @package was removed, or %FALSE if it was not found.
 */
gboolean pacman_manager_remove_sync_first (PacmanManager *manager, const gchar *package) {
	PacmanManagerPrivate *priv;
	gchar *removed;
	
	g_return_val_if_fail (manager != NULL, FALSE);
	g_return_val_if_fail (package != NULL, FALSE);
	
	priv = PACMAN_MANAGER_GET_PRIVATE (manager);
	priv->sync_firsts = pacman_list_remove_string (priv->sync_firsts, package, &removed);
	
	if (removed != NULL) {
		g_free (removed);
		return TRUE;
	} else {
		return FALSE;
	}
}

/**
 * pacman_manager_set_sync_firsts:
 * @manager: A #PacmanManager.
 * @sync_firsts: A list of package names.
 *
 * Sets the list of SyncFirsts to @sync_firsts. See pacman_manager_get_sync_firsts().
 */
void pacman_manager_set_sync_firsts (PacmanManager *manager, PacmanList *sync_firsts) {
	PacmanManagerPrivate *priv;
	
	g_return_if_fail (manager != NULL);
	
	priv = PACMAN_MANAGER_GET_PRIVATE (manager);
	pacman_list_free_full (priv->sync_firsts, (GDestroyNotify) g_free);
	priv->sync_firsts = pacman_list_strdup (sync_firsts);
}

/**
 * pacman_manager_get_ignore_groups:
 * @manager: A #PacmanManager.
 *
 * Gets a list of groups whose packages will never be upgraded.
 *
 * Returns: A list of group names. Do not free.
 */
const PacmanList *pacman_manager_get_ignore_groups (PacmanManager *manager) {
	g_return_val_if_fail (manager != NULL, NULL);
	
	return alpm_option_get_ignoregrps ();
}

/**
 * pacman_manager_add_ignore_group:
 * @manager: A #PacmanManager.
 * @group: The name of a group.
 *
 * Adds @group to the list of IgnoreGroups. See pacman_manager_get_ignore_groups().
 */
void pacman_manager_add_ignore_group (PacmanManager *manager, const gchar *group) {
	g_return_if_fail (manager != NULL);
	g_return_if_fail (group != NULL);
	
	alpm_option_add_ignoregrp (group);
}

/**
 * pacman_manager_remove_ignore_group:
 * @manager: A #PacmanManager.
 * @group: The name of a group.
 *
 * Removes @group from the list of IgnoreGroups. See pacman_manager_get_ignore_groups().
 *
 * Returns: %TRUE if @group was removed, or %FALSE if it was not found.
 */
gboolean pacman_manager_remove_ignore_group (PacmanManager *manager, const gchar *group) {
	g_return_val_if_fail (manager != NULL, FALSE);
	g_return_val_if_fail (group != NULL, FALSE);
	
	return alpm_option_remove_ignoregrp (group);
}

/**
 * pacman_manager_set_ignore_groups:
 * @manager: A #PacmanManager.
 * @ignore_groups: A list of group names.
 *
 * Sets the list of IgnoreGroups to @ignore_groups. See pacman_manager_get_ignore_groups().
 */
void pacman_manager_set_ignore_groups (PacmanManager *manager, PacmanList *ignore_groups) {
	g_return_if_fail (manager != NULL);
	
	alpm_option_set_ignoregrps (alpm_list_strdup (ignore_groups));
}

/**
 * pacman_manager_get_ignore_packages:
 * @manager: A #PacmanManager.
 *
 * Gets a list of packages that will never be upgraded.
 *
 * Returns: A list of package names. Do not free.
 */
const PacmanList *pacman_manager_get_ignore_packages (PacmanManager *manager) {
	g_return_val_if_fail (manager != NULL, NULL);
	
	return alpm_option_get_ignorepkgs ();
}

/**
 * pacman_manager_add_ignore_package:
 * @manager: A #PacmanManager.
 * @package: The name of a package.
 *
 * Adds @package to the list of IgnorePkgs. See pacman_manager_get_ignore_packages().
 */
void pacman_manager_add_ignore_package (PacmanManager *manager, const gchar *package) {
	g_return_if_fail (manager != NULL);
	g_return_if_fail (package != NULL);
	
	alpm_option_add_ignorepkg (package);
}

/**
 * pacman_manager_remove_ignore_package:
 * @manager: A #PacmanManager.
 * @package: The name of a package.
 *
 * Removes @package from the list of IgnorePkgs. See pacman_manager_get_ignore_packages().
 *
 * Returns: %TRUE if @package was removed, or %FALSE if it was not found.
 */
gboolean pacman_manager_remove_ignore_package (PacmanManager *manager, const gchar *package) {
	g_return_val_if_fail (manager != NULL, FALSE);
	g_return_val_if_fail (package != NULL, FALSE);
	
	return alpm_option_remove_ignorepkg (package);
}

/**
 * pacman_manager_set_ignore_packages:
 * @manager: A #PacmanManager.
 * @ignore_packages: A list of package names.
 *
 * Sets the list of IgnorePkgs to @ignore_packages. See pacman_manager_get_ignore_packages().
 */
void pacman_manager_set_ignore_packages (PacmanManager *manager, PacmanList *ignore_packages) {
	g_return_if_fail (manager != NULL);
	
	alpm_option_set_ignorepkgs (alpm_list_strdup (ignore_packages));
}

/**
 * pacman_manager_get_no_extracts:
 * @manager: A #PacmanManager.
 *
 * Gets a list of files that will not be extracted from any package containing one when that package is installed.
 *
 * Returns: A list of file names (without a leading slash). Do not free.
 */
const PacmanList *pacman_manager_get_no_extracts (PacmanManager *manager) {
	g_return_val_if_fail (manager != NULL, NULL);
	
	return alpm_option_get_noextracts ();
}

/**
 * pacman_manager_add_no_extract:
 * @manager: A #PacmanManager.
 * @filename: The location of a file (without a leading slash).
 *
 * Adds @filename to the list of NoExtracts. See pacman_manager_get_no_extracts().
 */
void pacman_manager_add_no_extract (PacmanManager *manager, const gchar *filename) {
	g_return_if_fail (manager != NULL);
	g_return_if_fail (filename != NULL);
	
	alpm_option_add_noextract (filename);
}

/**
 * pacman_manager_remove_no_extract:
 * @manager: A #PacmanManager.
 * @filename: The location of a file (without a leading slash).
 *
 * Removes @filename from the list of NoExtracts. See pacman_manager_get_no_extracts().
 *
 * Returns: %TRUE if @filename was removed, or %FALSE if it was not found.
 */
gboolean pacman_manager_remove_no_extract (PacmanManager *manager, const gchar *filename) {
	g_return_val_if_fail (manager != NULL, FALSE);
	g_return_val_if_fail (filename != NULL, FALSE);
	
	return alpm_option_remove_noextract (filename);
}

/**
 * pacman_manager_set_no_extracts:
 * @manager: A #PacmanManager.
 * @no_extracts: A list of file locations (without a leading slash).
 *
 * Sets the list of NoExtracts to @no_extracts. See pacman_manager_get_no_extracts().
 */
void pacman_manager_set_no_extracts (PacmanManager *manager, PacmanList *no_extracts) {
	g_return_if_fail (manager != NULL);
	
	alpm_option_set_noextracts (alpm_list_strdup (no_extracts));
}

/**
 * pacman_manager_get_no_upgrades:
 * @manager: A #PacmanManager.
 *
 * Gets a list of files that will not be changed when a package containing one is upgraded (any attempt to do so will create a .pacnew file instead).
 *
 * Returns: A list of file names (without a leading slash). Do not free.
 */
const PacmanList *pacman_manager_get_no_upgrades (PacmanManager *manager) {
	g_return_val_if_fail (manager != NULL, NULL);
	
	return alpm_option_get_noupgrades ();
}

/**
 * pacman_manager_add_no_upgrade:
 * @manager: A #PacmanManager.
 * @filename: A file name (without a leading slash).
 *
 * Adds @filename to the list of NoUpgrades. See pacman_manager_get_no_upgrades().
 */
void pacman_manager_add_no_upgrade (PacmanManager *manager, const gchar *filename) {
	g_return_if_fail (manager != NULL);
	g_return_if_fail (filename != NULL);
	
	alpm_option_add_noupgrade (filename);
}

/**
 * pacman_manager_remove_no_upgrade:
 * @manager: A #PacmanManager.
 * @filename: A file name (without a leading slash).
 *
 * Removes @filename from the list of NoUpgrades. See pacman_manager_get_no_upgrades().
 *
 * Returns: %TRUE if @filename was removed, or %FALSE if it was not found.
 */
gboolean pacman_manager_remove_no_upgrade (PacmanManager *manager, const gchar *filename) {
	g_return_val_if_fail (manager != NULL, FALSE);
	g_return_val_if_fail (filename != NULL, FALSE);
	
	return alpm_option_remove_noupgrade (filename);
}

/**
 * pacman_manager_set_no_upgrades:
 * @manager: A #PacmanManager.
 * @no_upgrades: A list of file names (without a leading slash).
 *
 * Sets the list of NoUpgrades to @no_upgrades. See pacman_manager_get_no_upgrades().
 */
void pacman_manager_set_no_upgrades (PacmanManager *manager, PacmanList *no_upgrades) {
	g_return_if_fail (manager != NULL);
	
	alpm_option_set_noupgrades (alpm_list_strdup (no_upgrades));
}

/**
 * pacman_manager_register_local_database:
 * @manager: A #PacmanManager.
 * @error: A #GError, or %NULL.
 *
 * Gets the local database, or registers one if it was not found.
 *
 * Returns: A #PacmanDatabase, or %NULL if @error is set.
 */
PacmanDatabase *pacman_manager_register_local_database (PacmanManager *manager, GError **error) {
	PacmanDatabase *database;
	
	g_return_val_if_fail (manager != NULL, NULL);
	
	database = pacman_manager_get_local_database (manager);
	if (database == NULL) {
		database = alpm_db_register_local ();
		
		if (database == NULL) {
			g_set_error (error, PACMAN_ERROR, pm_errno, _("Could not register database [local]: %s"), alpm_strerrorlast ());
			return NULL;
		}
	}
	
	return database;
}

/**
 * pacman_manager_register_sync_database:
 * @manager: A #PacmanManager.
 * @name: The name of the a sync database.
 * @error: A #GError, or %NULL.
 *
 * Searches for a sync database named @name, and registers a new one if none were found.
 *
 * Returns: A #PacmanDatabase, or %NULL if @error is set.
 */
PacmanDatabase *pacman_manager_register_sync_database (PacmanManager *manager, const gchar *name, GError **error) {
	PacmanDatabase *database;
	
	g_return_val_if_fail (manager != NULL, NULL);
	g_return_val_if_fail (name != NULL, NULL);
	
	database = alpm_db_register_sync (name);
	if (database == NULL) {
		g_set_error (error, PACMAN_ERROR, pm_errno, _("Could not register database [%s]: %s"), name, alpm_strerrorlast ());
		return NULL;
	}
	
	return database;
}

/**
 * pacman_manager_get_local_database:
 * @manager: A #PacmanManager.
 *
 * Gets the local database.
 *
 * Returns: A #PacmanDatabase, or %NULL if no local database is registered.
 */
PacmanDatabase *pacman_manager_get_local_database (PacmanManager *manager) {
	g_return_val_if_fail (manager != NULL, NULL);
	
	return alpm_option_get_localdb ();
}

/**
 * pacman_manager_get_sync_databases:
 * @manager: A #PacmanManager.
 *
 * Gets a list of registered sync databases.
 *
 * Returns: A list of #PacmanDatabase. Do not free.
 */
const PacmanList *pacman_manager_get_sync_databases (PacmanManager *manager) {
	g_return_val_if_fail (manager != NULL, NULL);
	
	return alpm_option_get_syncdbs ();
}

/**
 * pacman_manager_find_sync_database:
 * @manager: A #PacmanManager.
 * @name: The name of a sync database.
 *
 * Finds a sync database named @name.
 *
 * Returns: A #PacmanDatabase, or %NULL if none were found.
 */
PacmanDatabase *pacman_manager_find_sync_database (PacmanManager *manager, const gchar *name) {
	const PacmanList *i;
	
	g_return_val_if_fail (manager != NULL, NULL);
	
	for (i = pacman_manager_get_sync_databases (manager); i != NULL; i = pacman_list_next (i)) {
		PacmanDatabase *database = (PacmanDatabase *) pacman_list_get (i);
		if (g_strcmp0 (name, pacman_database_get_name (database)) == 0) {
			return database;
		}
	}
	
	return NULL;
}

/**
 * pacman_manager_unregister_database:
 * @manager: A #PacmanManager.
 * @database: A #PacmanDatabase.
 * @error: A #GError, or %NULL.
 *
 * Unregisters and frees @database.
 *
 * Returns: %TRUE if @database was unregistered, or %FALSE if @error is set.
 */
gboolean pacman_manager_unregister_database (PacmanManager *manager, PacmanDatabase *database, GError **error) {
	g_return_val_if_fail (manager != NULL, FALSE);
	
	if (alpm_db_unregister (database) < 0) {
		g_set_error (error, PACMAN_ERROR, pm_errno, _("Could not unregister database: %s"), alpm_strerrorlast ());
		return FALSE;
	}
	
	return TRUE;
}

/**
 * pacman_manager_unregister_all_databases:
 * @manager: A #PacmanManager.
 * @error: A #GError, or %NULL.
 *
 * Unregisters and frees every #PacmanDatabase object known to pacman.
 *
 * Returns: %TRUE if all databases were unregistered, or %FALSE if @error is set.
 */
gboolean pacman_manager_unregister_all_databases (PacmanManager *manager, GError **error) {
	g_return_val_if_fail (manager != NULL, FALSE);
	
	if (alpm_db_unregister_all () < 0) {
		g_set_error (error, PACMAN_ERROR, pm_errno, _("Could not unregister all databases: %s"), alpm_strerrorlast ());
		return FALSE;
	}
	
	return TRUE;
}

enum {
	PROP_0,
	PROP_VERSION,
	PROP_TRANSACTION
};

/**
 * pacman_manager_get_version:
 * @manager: A #PacmanManager.
 *
 * Gets the version of alpm in use.
 *
 * Returns: A version string. Do not free.
 */
const char *pacman_manager_get_version (PacmanManager *manager) {
	g_return_val_if_fail (manager != NULL, NULL);
	
	return alpm_version ();
}

/**
 * pacman_manager_get_transaction:
 * @manager: A #PacmanManager.
 *
 * Gets the current transaction.
 *
 * Returns: A #PacmanTransaction, or %NULL if none are in progress. This will be set to %NULL when the current transaction is destroyed. Do not free.
 */
PacmanTransaction *pacman_manager_get_transaction (PacmanManager *manager) {
	PacmanManagerPrivate *priv;
	
	g_return_val_if_fail (manager != NULL, NULL);
	
	priv = PACMAN_MANAGER_GET_PRIVATE (manager);
	return priv->transaction;
}

PacmanTransaction *pacman_manager_new_transaction (PacmanManager *manager, GType type) {
	PacmanManagerPrivate *priv;
	
	g_return_val_if_fail (manager != NULL, NULL);
	g_return_val_if_fail (pacman_manager_get_local_database (manager) != NULL, NULL);
	
	priv = PACMAN_MANAGER_GET_PRIVATE (manager);
	
	if (type == G_TYPE_NONE) {
		priv->transaction = NULL;
		return NULL;
	} else if (priv->transaction != NULL) {
		g_object_unref (priv->transaction);
	}
	
	priv->transaction = PACMAN_TRANSACTION (g_object_new (type, NULL));
	return priv->transaction;
}

static void pacman_manager_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec) {
	g_return_if_fail (object != NULL);
	g_return_if_fail (value != NULL);
	
	PacmanManager *manager;
	manager = PACMAN_MANAGER (object);
	
	switch (prop_id) {
		case PROP_VERSION:
			g_value_set_string (value, pacman_manager_get_version (manager));
			break;
		
		case PROP_TRANSACTION:
			g_value_set_instance (value, pacman_manager_get_transaction (manager));
			break;
		
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

/**
 * pacman_manager_find_missing_dependencies:
 * @manager: A #PacmanManager.
 * @remove: A list of #PacmanPackage.
 * @install: A list of #PacmanPackage.
 *
 * Finds dependencies of installed packages that will no longer be satisfied once the packages in @remove are removed and those in @install are installed.
 *
 * Returns: A list of #PacmanMissingDependency. Free the contents with pacman_missing_dependency_free(), then free the list with pacman_list_free().
 */
PacmanList *pacman_missing_dependency_check_transaction (PacmanManager *manager, const PacmanList *remove, const PacmanList *install) {
	PacmanDatabase *database;
	
	g_return_val_if_fail (manager != NULL, NULL);
	
	database = pacman_manager_get_local_database (manager);
	g_return_val_if_fail (database != NULL, NULL);
	
	return alpm_checkdeps ((PacmanList *) pacman_database_get_packages (database), 1, (PacmanList *) remove, (PacmanList *) install);
}

/**
 * pacman_manager_test_dependencies:
 * @manager: A #PacmanManager.
 * @dependencies: A list of strings representing dependencies. See pacman_dependency_to_string().
 *
 * Tests each dependency in @dependencies to determine whether it is satisfied by an installed package.
 *
 * Returns: A list of strings from @dependencies, corresponding to the dependencies which were not satisfied. Free with pacman_list_free().
 */
PacmanList *pacman_manager_test_dependencies (PacmanManager *manager, const PacmanList *dependencies) {
	PacmanDatabase *database;
	
	g_return_val_if_fail (manager != NULL, NULL);
	
	database = pacman_manager_get_local_database (manager);
	g_return_val_if_fail (database != NULL, NULL);
	
	return alpm_deptest (database, (PacmanList *) dependencies);
}

static void pacman_manager_class_init (PacmanManagerClass *klass) {
	g_return_if_fail (klass != NULL);
	
	g_type_class_add_private (klass, sizeof (PacmanManagerPrivate));
	
	G_OBJECT_CLASS (klass)->get_property = pacman_manager_get_property;
	G_OBJECT_CLASS (klass)->finalize = pacman_manager_finalize;
	
	/**
	 * PacmanManager:version:
	 *
	 * The version of alpm in use.
	 */
	g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_VERSION, g_param_spec_string ("version", _("Version"), _("alpm version"), NULL, G_PARAM_STATIC_NAME | G_PARAM_READABLE));
	
	/**
	 * PacmanManager:transaction:
	 *
	 * The current transaction, created by pacman_manager_install(), pacman_manager_remove(), pacman_manager_sync() or pacman_manager_update().
	 */
	g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_TRANSACTION, g_param_spec_object ("transaction", _("Transaction"), _("current transaction"), PACMAN_TYPE_TRANSACTION, G_PARAM_STATIC_NAME | G_PARAM_READABLE));
}