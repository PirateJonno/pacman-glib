#include <stdlib.h>
#include <glib/gi18n-lib.h>
#include <alpm.h>
#include "pacman-error.h"
#include "pacman-list.h"
#include "pacman-private.h"
#include "pacman-install.h"

/**
 * SECTION:pacman-install
 * @title: PacmanInstall
 * @short_description: Install packages from files
 *
 * A #PacmanInstall transaction can be used to install packages from local or remote files (equivalent to pacman -U). To do so, pass a list of file locations and/or URLs as targets to pacman_transaction_prepare(). Currently, missing dependencies for these packages will not be installed automatically.
 */

/**
 * PacmanInstall:
 *
 * Represents an installation transaction.
 */

G_DEFINE_TYPE (PacmanInstall, pacman_install, PACMAN_TYPE_TRANSACTION);

static void pacman_install_init (PacmanInstall *install) {	
	g_return_if_fail (install != NULL);
}

static void pacman_install_finalize (GObject *object) {
	g_return_if_fail (object != NULL);
	
	G_OBJECT_CLASS (pacman_install_parent_class)->finalize (object);
}

/**
 * pacman_manager_install:
 * @manager: A #PacmanManager.
 * @flags: A set of #PacmanTransactionFlags.
 * @error: A #GError, or %NULL.
 *
 * Initializes a transaction that can install packages from files, configured according to @flags.
 *
 * Returns: A #PacmanInstall transaction, or %NULL if @error is set. Free with g_object_unref().
 */
PacmanTransaction *pacman_manager_install (PacmanManager *manager, guint32 flags, GError **error) {
	PacmanTransaction *result;
	
	g_return_val_if_fail (manager != NULL, NULL);
	
	if (!pacman_transaction_start (PM_TRANS_TYPE_UPGRADE, flags, error)) {
		return NULL;
	}
	
	return pacman_manager_new_transaction (manager, PACMAN_TYPE_INSTALL);
}

static gboolean pacman_install_prepare (PacmanTransaction *transaction, const PacmanList *targets, GError **error) {
	const PacmanList *i;
	PacmanList *data = NULL;
	
	g_return_val_if_fail (transaction != NULL, FALSE);
	g_return_val_if_fail (targets != NULL, FALSE);
	
	if (pacman_transaction_get_packages (transaction) != NULL) {
		/* reinitialize so transaction can be prepared multiple times */
		if (!pacman_transaction_restart (transaction, error)) {
			return FALSE;
		}
	}
	
	for (i = targets; i != NULL; i = pacman_list_next (i)) {
		int result;
		gchar *target = (gchar *) pacman_list_get (i);
		
		if (strstr (target, "://") != NULL) {
			target = alpm_fetch_pkgurl (target);
			if (target == NULL) {
				g_set_error (error, PACMAN_ERROR, pm_errno, _("Could not download package with url '%s': %s"), (gchar *) pacman_list_get (i), alpm_strerrorlast ());
				return FALSE;
			}
			
			result = alpm_trans_addtarget (target);
			free (target);
		} else {
			result = alpm_trans_addtarget (target);
		}
		
		if (result < 0) {
			g_set_error (error, PACMAN_ERROR, pm_errno, _("Could not mark the file '%s' for installation: %s"), target, alpm_strerrorlast ());
			return FALSE;
		}
	}
	
	if (alpm_trans_prepare (&data) < 0) {
		if (pm_errno == PACMAN_ERROR_DEPENDENCY_UNSATISFIED) {
			/* TODO: offer option to sync dependencies first? (like makepkg -s) */
			pacman_transaction_set_missing_dependencies (transaction, data);
		} else if (pm_errno == PACMAN_ERROR_CONFLICT) {
			pacman_transaction_set_conflicts (transaction, data);
		} else if (pm_errno == PACMAN_ERROR_FILE_CONFLICT) {
			pacman_transaction_set_file_conflicts (transaction, data);
		} else if (data != NULL) {
			g_debug ("Possible memory leak for install error: %s\n", alpm_strerrorlast ());
		}
		
		g_set_error (error, PACMAN_ERROR, pm_errno, _("Could not prepare transaction: %s"), alpm_strerrorlast ());
		return FALSE;
	}
	
	return TRUE;
}

static void pacman_install_class_init (PacmanInstallClass *klass) {
	g_return_if_fail (klass != NULL);
	
	G_OBJECT_CLASS (klass)->finalize = pacman_install_finalize;
	
	PACMAN_TRANSACTION_CLASS (klass)->prepare = pacman_install_prepare;
}