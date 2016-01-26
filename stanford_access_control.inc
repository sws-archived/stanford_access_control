<?php
/**
 * Helper and utility functions
 */

/**
 * Helper function to check whether the path is protected or not.
 *
 * @param string $normal_path
 *   The aliased path of the current page.
 * @param string $alias
 *   The menu router path for the current page.
 * @param boolean $keys
 *   True if the only thing the caller wants is an array of the pids.
 *
 * @return mixed
 *   FALSE if the page is UNLOCKED.
 *   An array of rule information.
 */
function stanford_access_control_get_pids_by_path($normal_path, $alias = NULL, $keys = FALSE) {

  // Get saved options from DB.
  $options = stanford_access_control_get_config_array();
  $pids = array();

  // If no options then nothing to do.
  if (!$options) {
    return FALSE;
  }

  // Loop through the options and try to match the paths.
  foreach ($options as $obj) {

    // Only for enabled types.
    if (!$obj->enabled) {
      continue;
    }

    $m1 = drupal_match_path($normal_path, $obj->path);
    $m2 = FALSE;
    if (module_exists("path")) {
      $m2 = drupal_match_path($alias, $obj->path);
    }

    if ($m1 || $m2) {
      $pids[$obj->pid] = $obj;
    }

  }

  // If no pid found return false.
  if (!$pids) {
    return FALSE;
  }

  // If the caller only wants the pids.
  if ($keys) {
    return array_keys($pids);
  }

  // Give back all information.
  return $pids;
}

/**
 * Gets and returns a tidy array of options from the database.
 *
 * @param int $pid
 *   The id of the row to return.
 *
 * @return array
 *     An array of options.
 */
function stanford_access_control_get_config_array($pid = NULL) {

  // Static cache this.
  $options = &drupal_static(__FUNCTION__);

  // If the cache is hot and the caller wants all of them.
  if (is_array($options) && count($options) && is_null($pid)) {
    return $options;
  }

  // If the cache is hot and the caller wants just one row.
  if (is_array($options) && !is_null($pid) && isset($options[$pid])) {
    return $options[$pid];
  }

  // No cache. Start from scratch.
  $results = db_select("stanford_access_control", "sac")
    ->fields("sac")
    ->orderBy("path", 'ASC')
    ->execute()
    ->fetchAllAssoc("pid");

  // We got some.
  if (count($results) && is_null($pid)) {
    return $results;
  }

  // For the specific row.
  if (count($results) && isset($results[$pid])) {
    return $results[$pid];
  }

  return FALSE;
}

/**
 * [stanford_access_control_deny_access description]
 * @return [type] [description]
 */
function stanford_access_control_deny_access() {
  drupal_access_denied();
  exit();
}
