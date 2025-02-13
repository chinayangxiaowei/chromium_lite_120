// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/ash/common/assert.js';

import {getFile} from '../../common/js/api.js';
import {ArrayDataModel} from '../../common/js/array_data_model.js';
import {getKeyModifiers} from '../../common/js/dom_utils.js';
import {isFakeEntry, isSameEntry} from '../../common/js/entry_utils.js';
import {strf} from '../../common/js/translations.js';
import {FileErrorToDomError, UserCanceledError} from '../../common/js/util.js';

import {FileFilter} from './directory_contents.js';
import {DirectoryModel} from './directory_model.js';
import {renameEntry, validateEntryName, validateFileName} from './file_rename.js';
import {FileSelectionHandler} from './file_selection.js';
import {ConfirmDialog} from './ui/dialogs.js';
import {FilesAlertDialog} from './ui/files_alert_dialog.js';
import {ListContainer} from './ui/list_container.js';
import {ListSelectionModel} from './ui/list_selection_model.js';

/**
 * Controller to handle naming.
 */
export class NamingController {
  /**
   * @param {!ListContainer} listContainer
   * @param {!FilesAlertDialog} alertDialog
   * @param {!ConfirmDialog} confirmDialog
   * @param {!DirectoryModel} directoryModel
   * @param {!FileFilter} fileFilter
   * @param {!FileSelectionHandler} selectionHandler
   */
  constructor(
      listContainer, alertDialog, confirmDialog, directoryModel, fileFilter,
      selectionHandler) {
    /** @private @const @type {!ListContainer} */
    this.listContainer_ = listContainer;

    /** @private @const @type {!FilesAlertDialog} */
    this.alertDialog_ = alertDialog;

    /** @private @const @type {!ConfirmDialog} */
    this.confirmDialog_ = confirmDialog;

    /** @private @const @type {!DirectoryModel} */
    this.directoryModel_ = directoryModel;

    /** @private @const @type {!FileFilter} */
    this.fileFilter_ = fileFilter;

    /** @private @const @type {!FileSelectionHandler} */
    this.selectionHandler_ = selectionHandler;

    /**
     * Whether the entry being renamed is a root of a removable
     * partition/volume.
     * @private @type {boolean}
     */
    this.isRemovableRoot_ = false;

    // @ts-ignore: error TS2304: Cannot find name 'VolumeInfo'.
    /** @private @type {?VolumeInfo} */
    this.volumeInfo_ = null;

    // Register events.
    this.listContainer_.renameInput.addEventListener(
        'keydown', this.onRenameInputKeyDown_.bind(this));
    this.listContainer_.renameInput.addEventListener(
        'blur', this.onRenameInputBlur_.bind(this));
  }

  /**
   * Verifies the user entered name for file or folder to be created or
   * renamed to. See also validateFileName.
   * Returns true immediately if the name is valid, else returns false
   * after the user has dismissed the error dialog.
   *
   * @param {!DirectoryEntry} parentEntry The URL of the parent directory entry.
   * @param {string} name New file or folder name.
   * @return {!Promise<boolean>} True if valid.
   * @private
   */
  async validateFileName(parentEntry, name) {
    try {
      await validateFileName(
          parentEntry, name, this.fileFilter_.isHiddenFilesVisible());
      return true;
    } catch (error) {
      // @ts-ignore: error TS18046: 'error' is of type 'unknown'.
      await this.alertDialog_.showAsync(/** @type {string} */ (error.message));
      return false;
    }
  }

  /**
   * @param {string} filename
   * @return {Promise<string>}
   */
  async validateFileNameForSaving(filename) {
    const directory =
        /** @type {DirectoryEntry} */ (
            this.directoryModel_.getCurrentDirEntry());
    const currentDirUrl = directory.toURL().replace(/\/?$/, '/');
    const fileUrl = currentDirUrl + encodeURIComponent(filename);

    try {
      const isValid = await this.validateFileName(directory, filename);
      if (!isValid) {
        throw new Error('Invalid filename.');
      }

      if (directory && isFakeEntry(directory)) {
        // Can't save a file into a fake directory.
        throw new Error('Cannot save into fake entry.');
      }

      await getFile(directory, filename, {create: false});
    } catch (error) {
      // @ts-ignore: error TS18046: 'error' is of type 'unknown'.
      if (error.name == FileErrorToDomError.NOT_FOUND_ERR) {
        // The file does not exist, so it should be ok to create a new file.
        return fileUrl;
      }

      // @ts-ignore: error TS18046: 'error' is of type 'unknown'.
      if (error.name == FileErrorToDomError.TYPE_MISMATCH_ERR) {
        // A directory is found. Do not allow to overwrite directory.
        this.alertDialog_.show(strf('DIRECTORY_ALREADY_EXISTS', filename));
        throw error;
      }

      // Unexpected error.
      // @ts-ignore: error TS18046: 'error' is of type 'unknown'.
      console.warn('File save failed: ' + error.code);
      throw error;
    }

    // An existing file is found. Show confirmation dialog to overwrite it.
    // If the user selects "OK", save it.
    return new Promise((fulfill, reject) => {
      this.confirmDialog_.show(
          strf('CONFIRM_OVERWRITE_FILE', filename), fulfill.bind(null, fileUrl),
          () => reject(new UserCanceledError('Canceled')));
    });
  }

  /**
   * @return {boolean}
   */
  isRenamingInProgress() {
    // @ts-ignore: error TS2339: Property 'currentEntry' does not exist on type
    // 'HTMLInputElement'.
    return !!this.listContainer_.renameInput.currentEntry;
  }

  /**
   * @param {boolean} isRemovableRoot Indicates whether the target is a
   *     removable volume root or not.
   * @param {?import("../../externs/volume_info.js").VolumeInfo} volumeInfo A
   *     volume information about the target entry. |volumeInfo| can be null if
   *     method is invoked on a folder that is in the tree view and is not root
   *     of an external drive.
   */
  initiateRename(isRemovableRoot = false, volumeInfo = null) {
    this.isRemovableRoot_ = isRemovableRoot;
    this.volumeInfo_ = this.isRemovableRoot_ ? assert(volumeInfo) : null;

    const selectedIndex = this.listContainer_.selectionModel.selectedIndex;
    const item =
        this.listContainer_.currentList.getListItemByIndex(selectedIndex);
    if (!item) {
      return;
    }
    const label = item.querySelector('.filename-label');
    const input = this.listContainer_.renameInput;
    const dataModel = /** @type {!ArrayDataModel} */ (
        this.listContainer_.currentList.dataModel);
    const currentEntry = dataModel.item(item.listIndex);

    // @ts-ignore: error TS18047: 'label' is possibly 'null'.
    input.value = label.textContent;
    item.setAttribute('renaming', '');
    // @ts-ignore: error TS18047: 'label.parentNode' is possibly 'null'.
    label.parentNode.appendChild(input);
    input.focus();

    const selectionEnd = input.value.lastIndexOf('.');
    if (currentEntry.isFile && selectionEnd !== -1) {
      input.selectionStart = 0;
      input.selectionEnd = selectionEnd;
    } else {
      input.select();
    }

    // This has to be set late in the process so we don't handle spurious
    // blur events.
    // @ts-ignore: error TS2339: Property 'currentEntry' does not exist on type
    // 'HTMLInputElement'.
    input.currentEntry = currentEntry;
    this.listContainer_.startBatchUpdates();
  }

  /**
   * Restores the item which is being renamed while refreshing the file list. Do
   * nothing if no item is being renamed or such an item disappeared.
   *
   * While refreshing file list it gets repopulated with new file entries.
   * There is not a big difference whether DOM items stay the same or not.
   * Except for the item that the user is renaming.
   */
  restoreItemBeingRenamed() {
    if (!this.isRenamingInProgress()) {
      return;
    }

    const dm = this.directoryModel_;
    const leadIndex = dm.getFileListSelection().leadIndex;
    if (leadIndex < 0) {
      return;
    }

    const leadEntry = /** @type {Entry} */ (dm.getFileList().item(leadIndex));
    if (!isSameEntry(
            // @ts-ignore: error TS2339: Property 'currentEntry' does not exist
            // on type 'HTMLInputElement'.
            this.listContainer_.renameInput.currentEntry, leadEntry)) {
      return;
    }

    const leadListItem = this.listContainer_.findListItemForNode(
        this.listContainer_.renameInput);
    if (this.listContainer_.currentListType == ListContainer.ListType.DETAIL) {
      this.listContainer_.table.updateFileMetadata(leadListItem, leadEntry);
    }
    // @ts-ignore: error TS2339: Property 'restoreLeadItem' does not exist on
    // type 'List'.
    this.listContainer_.currentList.restoreLeadItem(leadListItem);
  }

  /**
   * @param {Event} event Key event.
   * @private
   */
  onRenameInputKeyDown_(event) {
    if (!this.isRenamingInProgress()) {
      return;
    }

    // Do not move selection or lead item in list during rename.
    // @ts-ignore: error TS2339: Property 'key' does not exist on type 'Event'.
    if (event.key === 'ArrowUp' || event.key === 'ArrowDown') {
      event.stopPropagation();
    }

    // @ts-ignore: error TS2339: Property 'key' does not exist on type 'Event'.
    switch (getKeyModifiers(event) + event.key) {
      case 'Escape':
        this.cancelRename_();
        event.preventDefault();
        break;

      case 'Enter':
        this.commitRename_();
        event.preventDefault();
        break;
    }
  }

  /**
   * @param {Event} event Blur event.
   * @private
   */
  // @ts-ignore: error TS6133: 'event' is declared but its value is never read.
  onRenameInputBlur_(event) {
    // @ts-ignore: error TS2551: Property 'contextMenu' does not exist on type
    // 'HTMLInputElement'. Did you mean 'oncontextmenu'?
    const contextMenu = this.listContainer_.renameInput.contextMenu;
    if (contextMenu && !contextMenu.hidden) {
      return;
    }

    if (this.isRenamingInProgress() &&
        // @ts-ignore: error TS2339: Property 'validation_' does not exist on
        // type 'HTMLInputElement'.
        !this.listContainer_.renameInput.validation_) {
      this.commitRename_();
    }
  }

  /**
   * @private
   * @return {!Promise<void>} Resolves when done renaming - both when renaming
   *     is
   * successful and when it fails.
   */
  async commitRename_() {
    const input = this.listContainer_.renameInput;
    // @ts-ignore: error TS2339: Property 'currentEntry' does not exist on type
    // 'HTMLInputElement'.
    const entry = input.currentEntry;
    const newName = input.value;

    const renamedItemElement = this.listContainer_.findListItemForNode(
        this.listContainer_.renameInput);
    const nameNode = renamedItemElement.querySelector('.filename-label');
    // @ts-ignore: error TS18047: 'nameNode' is possibly 'null'.
    if (!newName || newName == nameNode.textContent) {
      this.cancelRename_();
      return;
    }

    const volumeInfo = this.volumeInfo_;
    const isRemovableRoot = this.isRemovableRoot_;

    try {
      // @ts-ignore: error TS2339: Property 'validation_' does not exist on type
      // 'HTMLInputElement'.
      input.validation_ = true;
      await validateEntryName(
          entry, newName, this.fileFilter_.isHiddenFilesVisible(), volumeInfo,
          isRemovableRoot);
    } catch (error) {
      // @ts-ignore: error TS18046: 'error' is of type 'unknown'.
      await this.alertDialog_.showAsync(/** @type {string} */ (error.message));

      // Cancel rename if it fails to restore focus from alert dialog.
      // Otherwise, just cancel the commitment and continue to rename.
      if (document.activeElement != input) {
        this.cancelRename_();
      }

      return;
    } finally {
      // @ts-ignore: error TS2339: Property 'validation_' does not exist on type
      // 'HTMLInputElement'.
      input.validation_ = false;
    }

    // Validation succeeded. Do renaming.
    // @ts-ignore: error TS2339: Property 'currentEntry' does not exist on type
    // 'HTMLInputElement'.
    this.listContainer_.renameInput.currentEntry = null;
    if (this.listContainer_.renameInput.parentNode) {
      this.listContainer_.renameInput.parentNode.removeChild(
          this.listContainer_.renameInput);
    }

    // Optimistically apply new name immediately to avoid flickering in
    // case of success.
    // @ts-ignore: error TS18047: 'nameNode' is possibly 'null'.
    nameNode.textContent = newName;

    try {
      const newEntry =
          await renameEntry(entry, newName, volumeInfo, isRemovableRoot);

      // RemovableRoot doesn't have a callback to report renaming is done.
      if (!isRemovableRoot) {
        await this.directoryModel_.onRenameEntry(entry, assert(newEntry));
      }

      const selectionModel = /** @type {!ListSelectionModel} */ (
          this.listContainer_.currentList.selectionModel);

      // Select new entry.
      selectionModel.selectedIndex =
          this.directoryModel_.getFileList().indexOf(newEntry);
      // Force to update selection immediately.
      this.selectionHandler_.onFileSelectionChanged();

      renamedItemElement.removeAttribute('renaming');
      this.listContainer_.endBatchUpdates();

      // Focus may go out of the list. Back it to the list.
      this.listContainer_.currentList.focus();
    } catch (error) {
      // Write back to the old name.
      // @ts-ignore: error TS18047: 'nameNode' is possibly 'null'.
      nameNode.textContent = entry.name;
      renamedItemElement.removeAttribute('renaming');
      this.listContainer_.endBatchUpdates();

      // Show error dialog.
      // @ts-ignore: error TS18046: 'error' is of type 'unknown'.
      this.alertDialog_.show(error.message);
    }
  }

  /**
   * @private
   */
  cancelRename_() {
    // @ts-ignore: error TS2339: Property 'currentEntry' does not exist on type
    // 'HTMLInputElement'.
    this.listContainer_.renameInput.currentEntry = null;

    const item = this.listContainer_.findListItemForNode(
        this.listContainer_.renameInput);
    if (item) {
      item.removeAttribute('renaming');
    }

    const parent = this.listContainer_.renameInput.parentNode;
    if (parent) {
      parent.removeChild(this.listContainer_.renameInput);
    }

    this.listContainer_.endBatchUpdates();

    // Focus may go out of the list. Back it to the list.
    this.listContainer_.currentList.focus();
  }
}
