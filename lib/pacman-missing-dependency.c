/* pacman-missing-dependency.c
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
#include <alpm.h>
#include "pacman-missing-dependency.h"

/**
 * SECTION:pacman-missing-dependency
 * @title: PacmanMissingDependency
 * @short_description: Missing dependencies
 *
 * A #PacmanMissingDependency represents a #PacmanDependency required by a #PacmanPackage that is either missing, or will be missing after a package is removed.
 */

/**
 * PacmanMissingDependency:
 *
 * Represents a dependency that is or will be missing.
 */

/**
 * pacman_missing_dependency_free:
 * @dependency: A #PacmanMissingDependency.
 *
 * Frees @dependency.
 */
void pacman_missing_dependency_free (PacmanMissingDependency *dependency) {
	/* this is a hack, but it's better than a memory leak */
	gchar *str = (gchar *) pacman_missing_dependency_get_causing_package (dependency);
	if (str != NULL) {
		free (str);
	}
	
	free ((gchar *) pacman_missing_dependency_get_package (dependency));
	pacman_dependency_free (pacman_missing_dependency_get_dependency (dependency));
	free (dependency);
}

/**
 * pacman_missing_dependency_get_package:
 * @dependency: A #PacmanMissingDependency.
 *
 * Gets the package that requires @dependency.
 *
 * Returns: A package name. Do not free.
 */
const gchar *pacman_missing_dependency_get_package (PacmanMissingDependency *dependency) {
	g_return_val_if_fail (dependency != NULL, NULL);
	
	return alpm_miss_get_target (dependency);
}

/**
 * pacman_missing_dependency_get_dependency:
 * @dependency: A #PacmanMissingDependency.
 *
 * Gets the dependency that is missing for @dependency.
 *
 * Returns: A #PacmanDependency. Do not free.
 */
PacmanDependency *pacman_missing_dependency_get_dependency (PacmanMissingDependency *dependency) {
	g_return_val_if_fail (dependency != NULL, NULL);
	
	return alpm_miss_get_dep (dependency);
}

/**
 * pacman_missing_dependency_get_causing_package:
 * @dependency: A #PacmanMissingDependency.
 *
 * Gets the package that currently provides @dependency.
 *
 * Returns: A package name, or %NULL if no package provides @dependency. Do not free.
 */
const gchar *pacman_missing_dependency_get_causing_package (PacmanMissingDependency *dependency) {
	g_return_val_if_fail (dependency != NULL, NULL);
	
	return alpm_miss_get_causingpkg (dependency);
}
