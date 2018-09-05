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
 * @param bool $keys
 *   True if the only thing the caller wants is an array of the pids.
 * @param bool $allow_disabled
 *   Allow for matching of disabled paths.
 *
 * @return mixed
 *   FALSE if the page is UNLOCKED.
 *   An array of rule information.
 */
function stanford_access_control_get_pids_by_path($normal_path, $alias = NULL, $keys = FALSE, $allow_disabled = FALSE) {

  // Get saved options from DB.
  $options = stanford_access_control_get_config_array();
  $pids = array();

  if (module_exists("path") && is_null($alias)) {
    $alias = drupal_get_path_alias($normal_path);
  }

  // If no options then nothing to do.
  if (!$options) {
    return FALSE;
  }

  // Loop through the options and try to match the paths.
  foreach ($options as $obj) {

    // Only for enabled types when not explicit.
    if (!$allow_disabled && !$obj->enabled) {
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

  // Unserialize some of the fun data we get back.
  if (count($results)) {
    foreach ($results as $k => $obj) {
      $obj->users = unserialize($obj->users);
      $obj->roles = unserialize($obj->roles);
      $results[$k] = $obj;
    }
  }

  // We got some.
  if (count($results) && is_null($pid)) {
    $options = $results;
    return $results;
  }

  // For the specific row.
  if (count($results) && isset($results[$pid])) {
    $options[$pid] = $results[$pid];
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
  // Do other stuff.
  exit();
}

/**
 * Delete a rule by pid.
 * @param int $pid
 *   The pid key to delete from the database
 * @param  bool $skip
 *   Skip the cache clears. Useful when doing many transactions.
 *
 * @return bool
 *   True if db_delete worked. False otherwise.
 */
function stanford_access_control_delete_rule($pid, $skip = FALSE) {

  global $user;

  if (!is_numeric($pid)) {
    throw new Exception("PID must be numeric");
  }

  $info = stanford_access_control_get_config_array($pid);

  watchdog(
    'stanford_access_control',
    '%user has deleted rule #%rid %title',
    array(
      "%user" => $user->name,
      "%rid" => $pid,
      "%title" => $info->title,
    ),
    WATCHDOG_NOTICE
  );

  // Do the delete.
  $status = db_delete('stanford_access_control')
    ->condition('pid', $pid)
    ->execute();

  if (!$skip) {
    // Must rebuild node_access table...
    stanford_access_control_rebuild_and_clear_caches();
  }

  return $status;
}

/**
 * Saves a new rule to the database.
 *
 * Assumes all validation has already happened. Writes to database.
 *
 * @param array $data
 *   An array of keyed data to pass to drupal_write_record.
 *
 * @return bool
 *   success or not.
 */
function stanford_access_control_create_rule($data) {

  global $user;
  $defaults = array(
    'path' => NULL,
    'title' => "Missing Title",
    'users' => array(),
    'roles' => $user->roles,
    'enabled' => 1,
    'editor' => $user->uid,
    'changed' => time(),
    'owner' => $user->uid,
    'created' => time(),
  );

  $data = array_merge($defaults, $data);

  // Filter out disabled roles.
  $data["roles"] = array_filter($data["roles"]);

  $status = drupal_write_record("stanford_access_control", $data);

  // Must rebuild node_access table...
  stanford_access_control_rebuild_and_clear_caches();

  return $status;
}

/**
 * Updates an existing rule.
 *
 * Assumes all validation has already happened. Writes to database.
 *
 * @param array $data
 *   An array of keyed data to pass to drupal_write_record.
 *
 * @param  bool $skip
 *   Skip the cache clears. Useful when doing multiple transactions.
 *
 * @return bool
 *   success or not.
 */
function stanford_access_control_update_rule($data, $skip = FALSE) {

  global $user;
  $keys = array("pid");
  $defaults = array(
    // 'path' => NULL,
    // 'title' => "Missing Title",
    // 'users' => array(),
    // 'roles' => $user->roles,
    'editor' => $user->uid,
    'changed' => time(),
    'pid' => NULL,
  );
  $data = array_merge($defaults, $data);

  // don't want to change some things.
  unset($data["created"]);
  unset($data["owner"]);

  $status = drupal_write_record("stanford_access_control", $data, $keys);

  if (!$skip) {
    // Must rebuild node_access table...
    stanford_access_control_rebuild_and_clear_caches();
  }

  return $status;

}

/**
 * When something changes we need to do some heavy lifting to get it to appear
 * right away.
 */
function stanford_access_control_rebuild_and_clear_caches() {
  // node_access_needs_rebuild(TRUE);
  node_access_rebuild(TRUE);
  menu_rebuild();
  if (module_exists("views")) {
    views_invalidate_cache();
  }
  drupal_flush_all_caches();
}
