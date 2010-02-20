#include <glib/gi18n-lib.h>
#include <alpm.h>
#include "pacman-error.h"
#include "pacman-list.h"
#include "pacman-package.h"
#include "pacman-group.h"
#include "pacman-database.h"
#include "pacman-manager.h"
#include "pacman-private.h"
#include "pacman-remove.h"

/**
 * SECTION:pacman-remove
 * @title: PacmanRemove
 * @short_description: Remove packages
 *
 * A #PacmanRemove transaction can be used to remove packages from the system (equivalent to pacman -R). To do so, pass a list of package and/or group names as targets to pacman_transaction_prepare(). If some of those packages are marked as HoldPkg, the operation will fail unless the user agrees to %PACMAN_TRANSACTION_QUESTION_REMOVE_HOLD_PACKAGES.
 */

/**
 * PacmanRemove:
 *
 * Represents a remove transaction.
 */

G_DEFINE_TYPE (PacmanRemove, pacman_remove, PACMAN_TYPE_TRANSACTION);

static void pacman_remove_init (PacmanRemove *remove) {
	g_return_if_fail (remove != NULL);
}

static void pacman_remove_finalize (GObject *object) {
	g_return_if_fail (object != NULL);
	
	G_OBJECT_CLASS (pacman_remove_parent_class)->finalize (object);
}

/**
 * pacman_manager_remove:
 * @manager: A #PacmanManager.
 * @flags: A set of #PacmanTransactionFlags.
 * @error: A #GError, or %NULL.
 *
 * Initializes a transaction that can remove packages from the system, configured according to @flags.
 *
 * Returns: A #PacmanRemove transaction, or %NULL if @error is set. Free with g_object_unref().
 */
PacmanTransaction *pacman_manager_remove (PacmanManager *manager, guint32 flags, GError **error) {
	PacmanTransaction *result;
	
	g_return_val_if_fail (manager != NULL, NULL);
	
	if (!pacman_transaction_start (PM_TRANS_TYPE_REMOVE, flags, error)) {
		return NULL;
	}
	
	return pacman_manager_new_transaction (manager, PACMAN_TYPE_REMOVE);
}

static gboolean pacman_remove_prepare (PacmanTransaction *transaction, const PacmanList *targets, GError **error) {
	const PacmanList *i;
	PacmanList *data = NULL;
	
	g_return_val_if_fail (transaction != NULL, FALSE);
	g_return_val_if_fail (targets != NULL, FALSE);
	g_return_val_if_fail (pacman_manager != NULL, FALSE);
	
	if (pacman_transaction_get_packages (transaction) != NULL) {
		/* reinitialize so transaction can be prepared multiple times */
		if (!pacman_transaction_restart (transaction, error)) {
			return FALSE;
		}
	}
	
	for (i = targets; i != NULL; i = pacman_list_next (i)) {
		const gchar *target = (const gchar *) pacman_list_get (i);
		
		if (alpm_trans_addtarget ((gchar *) target) < 0) {
			if (pm_errno == PACMAN_ERROR_PACKAGE_NOT_FOUND) {
				PacmanDatabase *database;
				PacmanGroup *group;
				
				database = pacman_manager_get_local_database (pacman_manager);
				g_return_val_if_fail (database != NULL, FALSE);
				group = pacman_database_find_group (database, target);
				
				if (group != NULL) {
					const PacmanList *j;
					for (j = pacman_group_get_packages (group); j != NULL; j = pacman_list_next (j)) {
						PacmanPackage *package = (PacmanPackage *) pacman_list_get (j);
						target = pacman_package_get_name (package);
						
						if (alpm_trans_addtarget ((gchar *) target) < 0) {
							/* report error further down */
							break;
						}
					}
					
					if (j == NULL) {
						/* added all packages in group */
						continue;
					}
				}
			}
			
			g_set_error (error, PACMAN_ERROR, pm_errno, _("Could not mark the package %s for removal: %s"), target, alpm_strerrorlast ());
			return FALSE;
		}
	}
	
	if (alpm_trans_prepare (&data) < 0) {
		if (pm_errno == PACMAN_ERROR_DEPENDENCY_UNSATISFIED) {
			pacman_transaction_set_missing_dependencies (transaction, data);
		} else if (data != NULL) {
			g_debug ("Possible memory leak for remove error: %s\n", alpm_strerrorlast ());
		}
		
		g_set_error (error, PACMAN_ERROR, pm_errno, _("Could not prepare transaction: %s"), alpm_strerrorlast ());
		return FALSE;
	}
	
	for (i = pacman_transaction_get_packages (transaction); i != NULL; i = pacman_list_next (i)) {
		PacmanPackage *package = (PacmanPackage *) pacman_list_get (i);
		const gchar *name = pacman_package_get_name (package);
		
		if (pacman_list_find_string (pacman_manager_get_hold_packages (pacman_manager), name) != NULL) {
			pacman_transaction_mark_package (transaction, package);
		}
	}
	
	if (pacman_transaction_get_marked_packages (transaction) != NULL) {
		gchar *packages = pacman_package_make_list (pacman_transaction_get_marked_packages (transaction));
		gboolean result = pacman_transaction_ask (transaction, PACMAN_TRANSACTION_QUESTION_REMOVE_HOLD_PACKAGES, _("The following packages are marked as held: %s. Do you want to remove them anyway?"), packages);
		pacman_transaction_set_marked_packages (transaction, NULL);
		g_free (packages);
		
		if (!result) {
			g_set_error (error, PACMAN_ERROR, PACMAN_ERROR_PACKAGE_HELD, _("Some packages were marked as held."));
			return FALSE;
		}
	}
	
	return TRUE;
}

static void pacman_remove_class_init (PacmanRemoveClass *klass) {
	g_return_if_fail (klass != NULL);
	
	G_OBJECT_CLASS (klass)->finalize = pacman_remove_finalize;
	
	PACMAN_TRANSACTION_CLASS (klass)->prepare = pacman_remove_prepare;
}
