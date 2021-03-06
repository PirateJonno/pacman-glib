/* pacman-package.c
 *
 * Copyright (C) 2010 Jonathan Conder <j@skurvy.no-ip.org>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdlib.h>
#include <glib/gi18n-lib.h>
#include <alpm.h>
#include "pacman-error.h"
#include "pacman-list.h"
#include "pacman-delta.h"
#include "pacman-database.h"
#include "pacman-private.h"
#include "pacman-package.h"

/**
 * SECTION:pacman-package
 * @title: PacmanPackage
 * @short_description: Packages
 *
 * A #PacmanPackage represents a package that can be installed on the system.
 */

/**
 * PacmanPackage:
 *
 * Represents a package that could be installed.
 */

/**
 * pacman_package_new_from_file:
 * @filename: A file name.
 * @full: %TRUE or %FALSE.
 * @error: A #GError, or %NULL.
 *
 * Creates a new package from the file at @filename, and loads detailed information about it immediately if @full is %TRUE.
 *
 * Returns: A #PacmanPackage, or %NULL if @error is set. Free with pacman_package_free().
 */
PacmanPackage *pacman_package_new_from_file (const gchar *filename, gboolean full, GError **error) {
	PacmanPackage *package = NULL;
	
	g_return_val_if_fail (filename != NULL, NULL);
	
	if (alpm_pkg_load (filename, (unsigned short) full, &package) < 0) {
		g_set_error (error, PACMAN_ERROR, pm_errno, _("Could not load package from '%s': %s"), filename, alpm_strerrorlast ());
		return NULL;
	}
	
	return package;
}

/**
 * pacman_package_free:
 * @package: A #PacmanPackage.
 *
 * Frees @package.
 */
void pacman_package_free (PacmanPackage *package) {
	if (package != NULL) {
		alpm_pkg_free (package);
	}
}

/**
 * pacman_package_get_name:
 * @package: A #PacmanPackage.
 *
 * Gets the name of @package.
 *
 * Returns: A package name. Do not free.
 */
const gchar *pacman_package_get_name (PacmanPackage *package) {
	g_return_val_if_fail (package != NULL, NULL);
	
	return alpm_pkg_get_name (package);
}

/**
 * pacman_package_get_database:
 * @package: A #PacmanPackage.
 *
 * Gets the database that @package originated from.
 *
 * Returns: A #PacmanDatabase, or %NULL if @package is not from a database. Do not free.
 */
PacmanDatabase *pacman_package_get_database (PacmanPackage *package) {
	g_return_val_if_fail (package != NULL, NULL);
	
	return alpm_pkg_get_db (package);
}

/**
 * pacman_package_get_filename:
 * @package: A #PacmanPackage.
 *
 * Gets the name of the file that contains @package.
 *
 * Returns: A file name, or %NULL if it is not known. Do not free.
 */
const gchar *pacman_package_get_filename (PacmanPackage *package) {
	g_return_val_if_fail (package != NULL, NULL);
	
	return alpm_pkg_get_filename (package);
}

/**
 * pacman_package_get_version:
 * @package: A #PacmanPackage.
 *
 * Gets the version of @package.
 *
 * Returns: A package version. Do not free.
 */
const gchar *pacman_package_get_version (PacmanPackage *package) {
	g_return_val_if_fail (package != NULL, NULL);
	
	return alpm_pkg_get_version (package);
}

/**
 * pacman_package_compare_version:
 * @a: A version string.
 * @b: A version string.
 *
 * Compares the versions @a and @b in accordance to the rules used by pacman.
 *
 * Returns: -1 if a is older than b, 1 if a is newer than b, or 0 if the versions are the same, 
 */
gint pacman_package_compare_version (const gchar *a, const gchar *b) {
	g_return_val_if_fail (a != NULL, (b == NULL) ? 0 : -1);
	g_return_val_if_fail (b != NULL, 1);
	
	return alpm_pkg_vercmp (a, b);
}

/**
 * pacman_package_will_upgrade:
 * @package: A local #PacmanPackage.
 * @update: A sync #PacmanPackage.
 *
 * Decides whether @update is newer than @package, or had the force flag set.
 *
 * Returns: %TRUE if @update will be chosen as an update for @package, or %FALSE otherwise.
 */
gboolean pacman_package_will_upgrade (PacmanPackage *package, PacmanPackage *update) {
	gint result;
	
	g_return_val_if_fail (package != NULL, FALSE);
	g_return_val_if_fail (update != NULL, FALSE);
	
	result = pacman_package_compare_version (pacman_package_get_version (package), pacman_package_get_version (update));
	
	if (result < 0) {
		return TRUE;
	} else if (result > 0) {
		return pacman_package_has_force (update);
	} else {
		return FALSE;
	}
}

/**
 * pacman_package_find_new_version:
 * @package: A #PacmanPackage.
 * @databases: A list of sync #PacmanDatabase to search.
 *
 * Finds a newer version of @package in @databases.
 *
 * Returns: A #PacmanPackage, or %NULL if none were found.
 */
PacmanPackage *pacman_package_find_new_version (PacmanPackage *package, const PacmanList *databases) {
	g_return_val_if_fail (package != NULL, NULL);
	
	return alpm_sync_newversion (package, (PacmanList *) databases);
}

/**
 * pacman_package_find_upgrade:
 * @package: A #PacmanPackage.
 * @databases: A list of sync #PacmanDatabase to search.
 *
 * Finds a newer version of or a replacement for @package in @databases.
 *
 * Returns: A #PacmanPackage, or %NULL if none were found.
 */
PacmanPackage *pacman_package_find_upgrade (PacmanPackage *package, const PacmanList *databases) {
	const gchar *name;
	const PacmanList *packages;
	
	g_return_val_if_fail (package != NULL, NULL);
	
	name = pacman_package_get_name (package);
	
	for (; databases != NULL; databases = pacman_list_next (databases)) {
		PacmanDatabase *database = (PacmanDatabase *) pacman_list_get (databases);
		PacmanPackage *upgrade = pacman_database_find_package (database, name);
		
		if (upgrade != NULL) {
			if (pacman_package_will_upgrade (package, upgrade)) {
				return upgrade;
			} else {
				return NULL;
			}
		} else {
			for (packages = pacman_database_get_packages (database); packages != NULL; packages = pacman_list_next (packages)) {
				upgrade = (PacmanPackage *) pacman_list_get (packages);
				if (pacman_list_find_string (pacman_package_get_replaces (upgrade), name) != NULL) {
					return upgrade;
				}
			}
		}
	}
	
	return NULL;
}

/**
 * pacman_package_get_arch:
 * @package: A #PacmanPackage.
 *
 * Gets the processor architecture that @package was built for.
 *
 * Returns: A processor architecture, or "any" if @package is architecture-independent. Do not free.
 */
const gchar *pacman_package_get_arch (PacmanPackage *package) {
	g_return_val_if_fail (package != NULL, NULL);
	
	return alpm_pkg_get_arch (package);
}

/**
 * pacman_package_get_description:
 * @package: A #PacmanPackage.
 *
 * Gets a description of @package.
 *
 * Returns: A string. Do not free.
 */
const gchar *pacman_package_get_description (PacmanPackage *package) {
	g_return_val_if_fail (package != NULL, NULL);
	
	return alpm_pkg_get_desc (package);
}

/**
 * pacman_package_get_groups:
 * @package: A #PacmanPackage.
 *
 * Gets a list of the groups that @package belongs to.
 *
 * Returns: A list of group names. Do not free.
 */
const PacmanList *pacman_package_get_groups (PacmanPackage *package) {
	g_return_val_if_fail (package != NULL, NULL);
	
	return alpm_pkg_get_groups (package);
}

/**
 * pacman_package_get_licenses:
 * @package: A #PacmanPackage.
 *
 * Gets a list of the licenses that @package is distributed under.
 *
 * Returns: A list of license names. Do not free.
 */
const PacmanList *pacman_package_get_licenses (PacmanPackage *package) {
	g_return_val_if_fail (package != NULL, NULL);
	
	return alpm_pkg_get_licenses (package);
}

/**
 * pacman_package_get_url:
 * @package: A #PacmanPackage.
 *
 * Gets the address of the project website for @package.
 *
 * Returns: A URL. Do not free.
 */
const gchar *pacman_package_get_url (PacmanPackage *package) {
	g_return_val_if_fail (package != NULL, NULL);
	
	return alpm_pkg_get_url (package);
}

/**
 * pacman_package_get_dependencies:
 * @package: A #PacmanPackage.
 *
 * Gets a list of the packages that @package depends on.
 *
 * Returns: A list of #PacmanDependency. Do not free.
 */
const PacmanList *pacman_package_get_dependencies (PacmanPackage *package) {
	g_return_val_if_fail (package != NULL, NULL);
	
	return alpm_pkg_get_depends (package);
}

/**
 * pacman_package_get_optional_dependencies:
 * @package: A #PacmanPackage.
 *
 * Gets a list of packages that unlock additional features of @package, but are not required by it.
 *
 * Returns: A list of 'package: description'. Do not free.
 */
const PacmanList *pacman_package_get_optional_dependencies (PacmanPackage *package) {
	g_return_val_if_fail (package != NULL, NULL);
	
	return alpm_pkg_get_optdepends (package);
}

/**
 * pacman_package_find_required_by:
 * @package: A #PacmanPackage.
 *
 * Finds a list of installed packages that depend on @package directly.
 *
 * Returns: A list of package names. Free the contents with g_free(), then free the list with pacman_list_free().
 */
PacmanList *pacman_package_find_required_by (PacmanPackage *package) {
	PacmanList *i, *result;
	
	g_return_val_if_fail (package != NULL, NULL);
	
	result = alpm_pkg_compute_requiredby (package);
	
	if (!g_mem_is_system_malloc ()) {
		for (i = result; i != NULL; i = pacman_list_next (i)) {
			gchar *temp = pacman_list_get (i);
			pacman_list_set (i, g_strdup (temp));
			free (temp);
		}
	}
	
	return result;
}

/**
 * pacman_package_get_conflicts:
 * @package: A #PacmanPackage.
 *
 * Gets a list of packages that conflict with @package.
 *
 * Returns: A list of package names. Do not free.
 */
const PacmanList *pacman_package_get_conflicts (PacmanPackage *package) {
	g_return_val_if_fail (package != NULL, NULL);
	
	return alpm_pkg_get_conflicts (package);
}

/**
 * pacman_package_get_provides:
 * @package: A #PacmanPackage.
 *
 * Gets a list of packages that are provided by @package.
 *
 * Returns: A list of 'package' or 'package=version'. Do not free.
 */
const PacmanList *pacman_package_get_provides (PacmanPackage *package) {
	g_return_val_if_fail (package != NULL, NULL);
	
	return alpm_pkg_get_provides (package);
}

/**
 * pacman_package_get_replaces:
 * @package: A #PacmanPackage.
 *
 * Gets a list of packages that are obsoleted by @package.
 *
 * Returns: A list of package names. Do not free.
 */
const PacmanList *pacman_package_get_replaces (PacmanPackage *package) {
	g_return_val_if_fail (package != NULL, NULL);
	
	return alpm_pkg_get_replaces (package);
}

/**
 * pacman_package_get_backup:
 * @package: A #PacmanPackage.
 *
 * Gets a list of files that should be backed up before @package is removed.
 *
 * Returns: A list of 'file(TAB)md5sum'. Do not free.
 */
const PacmanList *pacman_package_get_backup (PacmanPackage *package) {
	g_return_val_if_fail (package != NULL, NULL);
	
	return alpm_pkg_get_backup (package);
}

/**
 * pacman_package_get_deltas:
 * @package: A #PacmanPackage.
 *
 * Gets a list of available delta patches for @package.
 *
 * Returns: A list of #PacmanDelta. Do not free.
 */
const PacmanList *pacman_package_get_deltas (PacmanPackage *package) {
	g_return_val_if_fail (package != NULL, NULL);
	
	return alpm_pkg_get_deltas (package);
}

/**
 * pacman_package_find_unused_deltas:
 * @package: A #PacmanPackage.
 *
 * Finds a list of delta patches that are no longer used by @package.
 *
 * Returns: A list of file names. Free with pacman_list_free().
 */
PacmanList *pacman_package_find_unused_deltas (PacmanPackage *package) {
	g_return_val_if_fail (package != NULL, NULL);
	
	return alpm_pkg_unused_deltas (package);
}

/**
 * pacman_package_get_files:
 * @package: A #PacmanPackage.
 *
 * Gets a list of the files and directories included in @package.
 *
 * Returns: A list of file names (without a leading slash). Do not free.
 */
const PacmanList *pacman_package_get_files (PacmanPackage *package) {
	g_return_val_if_fail (package != NULL, NULL);
	
	return alpm_pkg_get_files (package);
}

/**
 * pacman_package_get_size:
 * @package: A #PacmanPackage.
 *
 * Gets the compressed size of @package.
 *
 * Returns: A file size, in bytes.
 */
goffset pacman_package_get_size (PacmanPackage *package) {
	g_return_val_if_fail (package != NULL, 0);
	
	return (goffset) alpm_pkg_get_size (package);
}

/**
 * pacman_package_get_download_size:
 * @package: A #PacmanPackage.
 *
 * Calculates the download size of @package. This is the same as the compressed size unless @package is cached or there are deltas available.
 *
 * Returns: A file size, in bytes.
 */
goffset pacman_package_get_download_size (PacmanPackage *package) {
	g_return_val_if_fail (package != NULL, 0);
	
	return (goffset) alpm_pkg_download_size (package);
}

/**
 * pacman_package_get_installed_size:
 * @package: A #PacmanPackage.
 *
 * Gets the size of the space that @package will consume once installed.
 *
 * Returns: A file size, in bytes.
 */
goffset pacman_package_get_installed_size (PacmanPackage *package) {
	g_return_val_if_fail (package != NULL, 0);
	
	return (goffset) alpm_pkg_get_isize (package);
}

/**
 * pacman_package_get_md5sum:
 * @package: A sync #PacmanPackage.
 *
 * Gets the MD5 sum that @package should satisfy.
 *
 * Returns: An MD5 sum, or %NULL if it is not known. Do not free.
 */
const gchar *pacman_package_get_md5sum (PacmanPackage *package) {
	g_return_val_if_fail (package != NULL, NULL);
	
	return alpm_pkg_get_md5sum (package);
}

/**
 * pacman_package_generate_md5sum:
 * @filename: A file name.
 * @error: A #GError, or %NULL.
 *
 * Generates an MD5 sum from the file at @filename.
 *
 * Returns: An MD5 sum, or %NULL if @error is set. Free with g_free().
 */
gchar *pacman_package_generate_md5sum (const gchar *filename, GError **error) {
	gchar *result, *temp;
	
	g_return_val_if_fail (filename != NULL, NULL);
	
	if (alpm_compute_md5sum (filename) == NULL) {
		g_set_error (error, PACMAN_ERROR, pm_errno, _("Could not generate the MD5 sum of '%s': %s"), filename, alpm_strerrorlast ());
		return NULL;
	}
	
	if (!g_mem_is_system_malloc ()) {
		temp = result;
		result = g_strdup (result);
		free (temp);
	}
	
	return result;
}

/**
 * pacman_package_check_md5sum:
 * @package: A sync #PacmanPackage.
 * @error: A #GError, or %NULL.
 *
 * Checks whether the MD5 sum of @package matches the one it is supposed to satisfy.
 *
 * Returns: %TRUE if the sums matched, and %FALSE if they do not match or @error is set.
 */
gboolean pacman_package_check_md5sum (PacmanPackage *package, GError **error) {
	g_return_val_if_fail (package != NULL, FALSE);
	
	if (alpm_pkg_checkmd5sum (package) == 0) {
		return TRUE;
	} else if (pm_errno != PACMAN_ERROR_PACKAGE_INVALID) {
		g_set_error (error, PACMAN_ERROR, pm_errno, _("Could not check MD5 sum: %s"), alpm_strerrorlast ());
	}
	
	return FALSE;
}

/**
 * pacman_package_has_filename:
 * @package: A #PacmanPackage.
 * @filename: A file name.
 *
 * Checks whether @filename contains or is a delta that provides @package. This could be useful determining what package is being downloaded from a #PacmanTransaction::download signal handler.
 *
 * Returns: %TRUE if @filename corresponds to @package, or %FALSE otherwise.
 */
gboolean pacman_package_has_filename (PacmanPackage *package, const gchar *filename) {
	const PacmanList *i;
	
	g_return_val_if_fail (package != NULL, FALSE);
	g_return_val_if_fail (filename != NULL, FALSE);
	
	if (g_strcmp0 (pacman_package_get_filename (package), filename) == 0) {
		return TRUE;
	}
	
	g_return_val_if_fail (pacman_manager != NULL, FALSE);
	
	if (!pacman_manager_get_use_delta (pacman_manager)) {
		return FALSE;
	}
	
	for (i = pacman_package_get_deltas (package); i != NULL; i = pacman_list_next (i)) {
		PacmanDelta *delta = (PacmanDelta *) pacman_list_get (i);
		
		if (g_strcmp0 (pacman_delta_get_patch_filename (delta), filename) == 0) {
			return TRUE;
		}
	}
	
	return FALSE;
}

/**
 * pacman_package_has_force:
 * @package: A #PacmanPackage.
 *
 * Checks whether @package should be installed in preference to one of a newer version.
 *
 * Returns: %TRUE if @package should be preferred, or %FALSE otherwise.
 */
gboolean pacman_package_has_force (PacmanPackage *package) {
	g_return_val_if_fail (package != NULL, FALSE);
	
	return alpm_pkg_has_force (package) != 0;
}

/**
 * pacman_package_has_install_scriptlet:
 * @package: A #PacmanPackage.
 *
 * Checks whether @package provides a script to run upon installation, upgrade and removal.
 *
 * Returns: %TRUE if @package provides a script, or %FALSE otherwise.
 */
gboolean pacman_package_has_install_scriptlet (PacmanPackage *package) {
	g_return_val_if_fail (package != NULL, FALSE);
	
	return alpm_pkg_has_scriptlet (package) != 0;
}

/**
 * pacman_package_was_explicitly_installed:
 * @package: A #PacmanPackage.
 *
 * Checks whether @package was installed explicitly or as a dependency for another package.
 *
 * Returns: %TRUE if @package was explicitly installed, or %FALSE otherwise.
 */
gboolean pacman_package_was_explicitly_installed (PacmanPackage *package) {
	g_return_val_if_fail (package != NULL, FALSE);
	
	return alpm_pkg_get_reason (package) != PM_PKG_REASON_DEPEND;
}

/**
 * pacman_package_get_packager:
 * @package: A #PacmanPackage.
 *
 * Gets the name and email of the packager who built package.
 *
 * Returns: A string of the form 'name &lt;email&gt;', or %NULL if the packager is unknown. Do not free.
 */
const gchar *pacman_package_get_packager (PacmanPackage *package) {
	g_return_val_if_fail (package != NULL, NULL);
	
	return alpm_pkg_get_packager (package);
}

/**
 * pacman_package_get_build_date:
 * @package: A #PacmanPackage.
 *
 * Gets the date upon which @package was built.
 *
 * Returns: A #time_t.
 */
time_t pacman_package_get_build_date (PacmanPackage *package) {
	g_return_val_if_fail (package != NULL, 0);
	
	return alpm_pkg_get_builddate (package);
}

/**
 * pacman_package_get_install_date:
 * @package: A #PacmanPackage.
 *
 * Gets the date upon which @package was installed.
 *
 * Returns: A #time_t.
 */
time_t pacman_package_get_install_date (PacmanPackage *package) {
	g_return_val_if_fail (package != NULL, 0);
	
	return alpm_pkg_get_installdate (package);
}

/**
 * pacman_package_read_changelog:
 * @package: A #PacmanPackage.
 *
 * Reads the changelog of @package.
 *
 * Returns: A string, or %NULL if @package does not provide a changelog. Free with g_free().
 */
gchar *pacman_package_read_changelog (PacmanPackage *package) {
	gpointer handle;
	
	g_return_val_if_fail (package != NULL, NULL);
	
	handle = alpm_pkg_changelog_open (package);
	if (handle == NULL) {
		return NULL;
	}
	
	gchar buffer[1024];
	gsize length;
	GString *result = g_string_new ("");
	
	while ((length = alpm_pkg_changelog_read (buffer, sizeof (buffer), package, handle)) != 0) {
		g_string_append_len (result, buffer, length);
	}
	
	alpm_pkg_changelog_close (package, handle);
	return g_string_free (result, FALSE);
}

/**
 * pacman_package_make_list:
 * @packages: A list of #PacmanPackage.
 *
 * Creates a list of package names from @packages.
 *
 * Returns: A comma-separated string. Free with g_free().
 */
gchar *pacman_package_make_list (const PacmanList *packages) {
	GString *result;
	const PacmanList *i = packages;
	
	if (i == NULL) {
		return g_strdup ("");
	} else {
		result = g_string_new (pacman_package_get_name ((PacmanPackage *) pacman_list_get (i)));
	}
	
	while ((i = pacman_list_next (i)) != NULL) {
		g_string_append_printf (result, _(", %s"), pacman_package_get_name (pacman_list_get (i)));
	}
	
	return g_string_free (result, FALSE);
}
