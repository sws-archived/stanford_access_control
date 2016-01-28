<?php
/**
 * @file
 *
 * The majority of this code was taken from and modified from the
 * Protected Pages Drupal project. https://www.drupal.org/project/stanford_access_control
 *
 */


/**
 * Callback function for add protected page.
 */
function stanford_access_control_configure($form, &$form_state) {

  // Confirmation step for delete:
  // ---------------------------------------------------------------------------

  if (isset($form_state["storage"]["confirm"])) {
    $question = "Are you sure you want to delete these protected paths?";
    $description = "These changes are permanent and cannot be undone";
    $path = current_path();
    return confirm_form($form, $question, $path, $description);
  }

  // Normal form
  // ---------------------------------------------------------------------------

  $form["rules_ops"]["create_new"]["#markup"] = "<p>" . l(t("+ Create new protected path"), "admin/config/stanford/access-control/new", array("attributes" => array("class" => "btn"))) . "</p>";

  $form["bulk_ops"] = array(
    "#title" => t("Bulk Operations"),
    "#type" => "fieldset",
    '#collapsible' => TRUE,
    '#collapsed' => FALSE,
  );

  $form["bulk_ops"]["operations"] = array(
    "#type" => "select",
    "#options" => array(
      'enable' => t("Enable"),
      'disable' => t("Disable"),
      'delete' => t("Delete"),
      'add_roles' => t("Add Roles"),
      'del_roles' => t("Remove Roles"),
    ),
  );

  $role_opts = user_roles();
  $role_opts_keyed = array_combine($role_opts, $role_opts);
  $form['bulk_ops']['roles'] = array(
    '#type' => 'checkboxes',
    '#title' => 'Roles that can view',
    '#options' => $role_opts_keyed,
    '#default_value' => isset($user->roles) ? $user->roles : "",
    '#states' => array(
      'visible' => array(
        array(':input[name="operations"]' => array('value' => "add_roles")),
        array(':input[name="operations"]' => array('value' => "del_roles")),
      ),
    )
  );

  $form["bulk_ops"]['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Apply'),
  );

  $form['rules_table'] = stanford_access_control_get_pages_list();

  $form["table_pager"]["#markup"] = theme('pager', array('tags' => array()));

  // Add some hooks.
  $form["#validate"][] = "stanford_access_control_configure_validate";
  $form["#submit"][] = "stanford_access_control_configure_submit";

  return $form;
}

/**
 * [stanford_access_control_configure_validate description]
 * @param  [type] &$form       [description]
 * @param  [type] &$form_state [description]
 * @return [type]              [description]
 */
function stanford_access_control_configure_validate(&$form, &$form_state) {
  // Nothing doing here.
}

/**
 * [stanford_access_control_configure_submit description]
 * @param  [type] $form        [description]
 * @param  [type] &$form_state [description]
 * @return [type]              [description]
 */
function stanford_access_control_configure_submit($form, &$form_state) {
  $values = $form_state["values"];

  // What is the op?
  $op = isset($values["operations"]) ? $values["operations"] : "";
  if (isset($form_state["values"]["confirm"]) && isset($form_state["storage"]["op"])) {
    $op = $form_state["storage"]["op"];
  }

  switch ($op) {
    case 'add_roles':
      stanford_access_control_configure_submit_add_roles($form_state);
      break;

    case 'del_roles':
      stanford_access_control_configure_submit_del_roles($form_state);
      break;

    case 'enable':
      stanford_access_control_configure_submit_enable($form_state);
      break;

    case 'disable':
      stanford_access_control_configure_submit_disable($form_state);
      break;

    case 'delete':
      $form_state['storage']['op'] = "delete_confirmed";
      $form_state['storage']['confirm'] = TRUE;
      $form_state['storage']['rules_table'] = array_filter($form_state["values"]['rules_table']);
      $form_state['rebuild'] = TRUE;
      break;

    case "delete_confirmed":
      stanford_access_control_configure_submit_delete($form_state['storage']['rules_table']);
      break;

    default:
      drupal_set_message("Nothing doing?", "error");
  }
}

/**
 * [stanford_access_control_configure_submit_add_roles description]
 * @param  [type] &$form_state [description]
 * @return [type]              [description]
 */
function stanford_access_control_configure_submit_add_roles(&$form_state) {
  $pids = array_filter($form_state["values"]["rules_table"]);
  $roles = array_filter($form_state["values"]["roles"]);

  if (empty($pids) || empty($roles)) {
    drupal_set_message("Please select both protected paths and roles.", "error");
    return;
  }

  foreach ($pids as $pid) {
    $rule = stanford_access_control_get_config_array($pid);
    $data = array(
      'pid' => $pid,
      'roles' => array_merge($rule->roles, $roles),
    );
    stanford_access_control_update_rule($data, TRUE);
  }

  // Must rebuild node_access table...
  stanford_access_control_rebuild_and_clear_caches();

  drupal_set_message("The protected paths have been modified.");

}

/**
 * [stanford_access_control_configure_submit_del_roles description]
 * @param  [type] &$form_state [description]
 * @return [type]              [description]
 */
function stanford_access_control_configure_submit_del_roles(&$form_state) {

  $pids = array_filter($form_state["values"]["rules_table"]);
  $roles = array_filter($form_state["values"]["roles"]);

  if (empty($pids) || empty($roles)) {
    drupal_set_message("Please select both protected paths and roles.", "error");
    return;
  }

  foreach ($pids as $pid) {
    $rule = stanford_access_control_get_config_array($pid);
    $data = array(
      'pid' => $pid,
      'roles' => array_diff($rule->roles, $roles),
    );
    stanford_access_control_update_rule($data, TRUE);
  }

  // Must rebuild node_access table...
  stanford_access_control_rebuild_and_clear_caches();

  drupal_set_message("The protected paths have been modified.");

}

/**
 * [stanford_access_control_configure_submit_enable description]
 * @param  [type] &$form_state [description]
 * @return [type]              [description]
 */
function stanford_access_control_configure_submit_enable(&$form_state) {
  $values = $form_state["values"];
  $pids = array_filter($values["rules_table"]);

  if (!count($pids)) {
    drupal_set_message("No items were selected.", "error");
    return;
  }

  $orq = db_or();
  foreach ($pids as $pid) {
    $orq->condition("pid", $pid);
  }

  // Bulk.
  db_update("stanford_access_control")
    ->fields(array(
      "enabled" => 1,
    ))
    ->condition($orq)
    ->execute();

  // Must rebuild node_access table...
  stanford_access_control_rebuild_and_clear_caches();

  drupal_set_message("The protected paths have been enabled.");
}

/**
 * [stanford_access_control_configure_submit_disable description]
 * @param  [type] &$form_state [description]
 * @return [type]              [description]
 */
function stanford_access_control_configure_submit_disable(&$form_state) {
  $values = $form_state["values"];
  $pids = array_filter($values["rules_table"]);

  if (!count($pids)) {
    drupal_set_message("No items were selected.", "error");
    return;
  }

  $orq = db_or();
  foreach ($pids as $pid) {
    $orq->condition("pid", $pid);
  }

  // Bulk.
  db_update("stanford_access_control")
    ->fields(array(
      "enabled" => 0,
    ))
    ->condition($orq)
    ->execute();

  // Must rebuild node_access table...
  stanford_access_control_rebuild_and_clear_caches();

  drupal_set_message("The protected paths have been disabled.", "status");
}

/**
 * [stanford_access_control_configure_submit_delete description]
 * @param  [type] &$form_state [description]
 * @return [type]              [description]
 */
function stanford_access_control_configure_submit_delete($pids) {
  $clean = array_filter($pids);

  // Need at least one item.
  if (empty($clean)) {
    drupal_set_message("No items were selected.", "error");
    return;
  }

  $orq = db_or();
  foreach ($clean as $pid) {
    $orq->condition("pid", $pid);
  }

  db_delete("stanford_access_control")
    ->condition($orq)
    ->execute();

  // Something changed. Clear clear clear!
  stanford_access_control_rebuild_and_clear_caches();

  drupal_set_message("The protected paths have been deleted.", "status");
}

/**
 * Callback to generate list of protected pages.
 */
function stanford_access_control_get_pages_list() {

  $header = array(
    array(
      'data' => t('#'),
      'field' => 'pid',
      'sort' => 'ASC',
    ),
    array(
      'data' => t('Admin Title'),
      'field' => 'title',
      'sort' => 'ASC',
    ),
    array(
      'data' => t('Protected Path(s)'),
      'field' => 'path',
      'sort' => 'ASC',
    ),
    array(
      'data' => t('Status'),
      'field' => 'enabled',
      'sort' => 'ASC',
    ),
    array(
      'data' => t('Modified Date'),
      'field' => 'changed',
      'sort' => 'ASC',
    ),
    array(
      'data' => t('Operations'),
      'colspan' => 2,
    ),
  );

  $rows = array();

  $query = db_select('stanford_access_control', 'sac')
    ->extend('PagerDefault')
    ->extend('TableSort');

  $query->fields('sac')
    ->limit(30)
    ->orderByHeader($header);

  if (!isset($_GET["order"])) {
    $query->orderBy("changed", "DESC");
  }

  $result = $query->execute();

  $rows = array();

  foreach ($result as $data) {
    $rows[$data->pid] = array(
      $data->pid,
      $data->title,
      $data->path,
      ($data->enabled) ? t("Enabled") : t("Disabled"),
      format_date($data->changed, "short"),
      l(t('Edit'), "admin/config/stanford/access-control/" . $data->pid . "/edit") . ' <span class=\"separator\">|</span> ' .
      l(t('Delete'), 'admin/config/stanford/access-control/' . $data->pid . "/delete"),
    );
  }

  return array(
    '#type' => 'tableselect',
    "#header" => $header,
    "#options" => $rows,
    "#sticky" => TRUE,
    "#js_select" => TRUE,
    "#caption" => "",
    "#colgroups" => array(),
    "#empty" => t("No records found!"),
  );

}

/**
 * [stanford_access_control_create_new_rule_form description]
 * @param  [type] $form        [description]
 * @param  [type] &$form_state [description]
 * @return [type]              [description]
 */
function stanford_access_control_create_new_rule_form($form, &$form_state) {

  stanford_access_control_get_rule_form($form, $form_state);

  // Let the validate and submit hooks know this is a new form.
  $form['is_new'] = array('#type' => 'value', '#value' => TRUE);

  $form["#validate"][] = "stanford_access_control_rule_validate";
  $form["#submit"][] = "stanford_access_control_rule_submit";

  return $form;
}

/**
 * Callback function for edit protected page form.
 */
function stanford_access_control_edit_rule_form($form, &$form_state, $pid) {

  if (!is_numeric($pid)) {
    drupal_set_message("PID must be numeric", "error");
    drupal_access_denied();
    exit;
  }

  // Get some data.
  $protected_page = stanford_access_control_get_config_array($pid);

  if (!isset($protected_page->path)) {
    drupal_set_message("No path found for that PID", "error");
    drupal_access_denied();
    exit;
  }

  // Get the shared form to start.
  stanford_access_control_get_rule_form($form, $form_state);

  // Alter a few elements.
  $form['rules_list']['path']["#default_value"] = $protected_page->path;
  $form['rules_list']['enabled']["#default_value"] = $protected_page->enabled;
  $form['rules_list']['admin_title']["#default_value"] = $protected_page->title;
  $form['rules_list']['roles']["#default_value"] = $protected_page->roles;

  // Add the pid.
  $form['rules_list']['pid'] = array(
    '#type' => 'value',
    '#value' => $pid,
  );

  // Add the validate and submit hooks.
  $form["#validate"][] = "stanford_access_control_rule_validate";
  $form["#submit"][] = "stanford_access_control_rule_submit";

  return $form;
}

/**
 * Alters the form to add the rule form common elements.
 * @param  [type] &$form       [description]
 * @param  [type] &$form_state [description]
 * @return [type]              [description]
 */
function stanford_access_control_get_rule_form(&$form, &$form_state) {

  global $user;
  $role_opts = user_roles();
  $role_opts_keyed = array_combine($role_opts, $role_opts);

  $form['rules_list'] = array(
    '#title' => t("Protected Page Relative path and password."),
    '#type' => 'fieldset',
    '#prefix' => '<div id="rules_list">',
    '#suffix' => '</div>',
  );

  $form['rules_list']['enabled'] = array(
    '#type' => 'checkbox',
    '#title' => t('Enable'),
    '#description' => t('Uncheck to disable this protected path.'),
    '#default_value' => 1,
  );

  $form['rules_list']['admin_title'] = array(
    '#type' => 'textfield',
    '#title' => t('Administrative title'),
    '#description' => t('Please enter an administrative title. This will only be used on admin pages.'),
    '#required' => TRUE,
  );

  $form['rules_list']['path'] = array(
    '#type' => 'textfield',
    '#title' => t('Relative Path'),
    '#description' => t('Please enter a relative path. This can be the path alias eg: "about-us" or the raw path eg: "node/115" as well as wildcard paths eg: academics*.'),
    '#required' => TRUE,
  );

  $form['rules_list']['roles'] = array(
    '#type' => 'checkboxes',
    '#title' => 'Roles that can view',
    '#description' => t("The roles selected here will be able to view the pages protected by this path rule"),
    '#options' => $role_opts_keyed,
    '#default_value' => isset($user->roles) ? $user->roles : "",
  );

  $form['rules_list']['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Save'),
  );

}

/**
 * Implements hook_validate().
 */
function stanford_access_control_rule_validate($form, &$form_state) {

  $path = "";
  $values = $form_state['values'];
  $raw_value = $values['path'];
  $normal_path = drupal_get_normal_path($raw_value);

  $patterns = array(
    '/(\r\n?|\n)/', // newlines
    '/\*/', // asterisks
    '/(^|\|)\<front\>($|\|)/' // <front>
  );

  // Check for pattern first.
  foreach ($patterns as $pattern) {
    if (preg_match($pattern, $raw_value)) {
      $path = $raw_value;
      break;
    }
  }

  // Validate normal path.
  if (empty($path)) {
    $path = drupal_strtolower(drupal_get_path_alias($values['path']));
    if (!drupal_valid_path($normal_path)) {
      form_set_error('path', t('Please enter a correct path!'));
    }
  }

  $pid = db_select('stanford_access_control')
      ->fields('stanford_access_control', array('pid'))
      ->condition(db_or()->condition('path', $normal_path)->condition('path', $path))
      ->range(0, 1)
      ->execute()
      ->fetchField();

  // If we find a match that is not ours deny it.
  if ($pid && $pid !== $values["pid"]) {
    form_set_error('path', t('Duplicate path entry is not allowed. There is already a path or its alias exists.'));
  }

}

/**
 * Ajax submit callback for add protected page form.
 */
function stanford_access_control_rule_submit($form, &$form_state) {

  $errors = form_get_errors();
  if (count($errors) >= 1) {
    return;
  }

  global $user;
  $values = $form_state['values'];
  $log_type = "update";

  $data = array(
    'path' => check_plain($values["path"]),
    'title' => check_plain($values["admin_title"]),
    'roles' => $values["roles"],
    'enabled' => $values["enabled"],
  );

  // If we are creating a new entry.
  if (isset($values["is_new"]) && $values["is_new"]) {

    // For the log.
    $log_type = "create";

    // Write to the db.
    stanford_access_control_create_rule($data);
  }
  // If we are updating an entry.
  else {
    // Must have pid as key.
    $data["pid"] = $values["pid"];
    stanford_access_control_update_rule($data);
  }

  // Let the user know everything is ok.
  drupal_set_message('The settings has been successfully saved.', 'status');

  // Log the change.
  stanford_access_control_log_event($log_type, $values);

  // The go somewhere.
  $form_state['redirect'] = 'admin/config/stanford/access-control';

}

/**
 * Writes a watchdog log of a change.
 * @param  [type] $form_values [description]
 * @return [type]              [description]
 */
function stanford_access_control_log_event($type, $data) {
  $link = NULL;
  $level = WATCHDOG_NOTICE;
  $variables = array();
  global $user;

  switch ($type) {
    case 'create':
      $message = "!user created new protection: @title";
      $variables = array(
        '!user' => l($user->name, "user/" . $user->uid),
        '@title' => $data["admin_title"],
      );
      break;

    case 'update':
      $message = "!user updated protection: @title";
      $variables = array(
        '!user' => l($user->name, "user/" . $user->uid),
        '@title' => $data["admin_title"],
      );
      $link = l(t("See page"), "admin/config/stanford/access-control/" . $data["pid"] . "/edit");
      break;

    case 'delete':
      $message = "!user deleted protection: @title";
      $variables = array(
        '!user' => l($user->name, "user/" . $user->uid),
        '@title' => $data["admin_title"],
      );
      break;

    case 'disable':
      $message = "!user disabled protection: @title";
      $variables = array(
        '!user' => l($user->name, "user/" . $user->uid),
        '@title' => $data["admin_title"],
      );
      $link = l(t("See page"), "admin/config/stanford/access-control/" . $data["pid"] . "/edit");
      break;

    case 'enable':
      $message = "!user enabled protection: @title";
      $variables = array(
        '!user' => l($user->name, "user/" . $user->uid),
        '@title' => $data["admin_title"],
      );
      $link = l(t("See page"), "admin/config/stanford/access-control/" . $data["pid"] . "/edit");
      break;
  }

  // Can't log no message. huyuck.
  if (!$message) {
    return FALSE;
  }

  watchdog("stanford_access_control", $message, $variables, $level, $link);
}


/**
 * Callback function for delete protected page.
 */
function stanford_access_control_delete_confirm($form, &$form_state, $pid) {

  if (!is_numeric($pid)) {
    throw new Exception("PID must be numeric.");
  }

  $data = stanford_access_control_get_config_array($pid);
  $path = $data->path;

  if (empty($path)) {
    throw new Exception("No path found for pid: $pid");
  }

  $form['pid'] = array(
    '#type' => 'value',
    '#value' => $pid,
  );

  $form["admin_title"] = array(
    '#type' => 'value',
    '#value' => $data->title,
  );

  $message = t('Are you sure you want to delete <b>"%path"</b> from protected pages list?', array('%path' => $path));
  $cancel = 'admin/config/stanford/access-control';
  $description = t('This action cannot be undone.');

  return confirm_form($form, $message, $cancel, $description, t('Delete'), t('Cancel'));
}

/**
 * Implements hook_submit().
 */
function stanford_access_control_delete_confirm_submit($form, &$form_state) {

  $values = $form_state["values"];

  // Die if not confirmed.
  if (!$values['confirm']) {
    return;
  }

  // Pid from the form submission.
  $pid = $values['pid'];

  stanford_access_control_delete_rule($pid);

  // Let the user know stuff happened.
  drupal_set_message(t('The path has been successfully deleted from the protected pages.'));

  // Log it.
  stanford_access_control_log_event("delete", array("admin_title" => $values["admin_title"]));

  // Got to go somewhere...
  $form_state['redirect'] = 'admin/config/stanford/access-control';
}

/**
 * Callback function for protected pages settings.
 */
function stanford_access_control_settings() {

  $form["sac_messages"] = array(
    '#type' => "checkbox",
    '#title' => "Enable protected messages",
    '#description' => "Check this box to show messages on pages that are protected",
    '#default_value' => variable_get("stanford_access_control_messages", TRUE),
  );

  // Add a submit handler.
  $form['#submit'][] = 'stanford_access_control_settings_submit';

  // System!
  return system_settings_form($form);
}

/**
 * Custom submit function encrypt password and deleting non-useful variable.
 */
function stanford_access_control_settings_submit($form, &$form_state) {

  $values = $form_state["values"];
  if ($values["sac_messages"]) {
    variable_set("stanford_access_control_messages", TRUE);
  }
  else {
    variable_set("stanford_access_control_messages", FALSE);
  }

}


