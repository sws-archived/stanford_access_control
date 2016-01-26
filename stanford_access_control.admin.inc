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

  $form["rules_ops"]["create_new"]["#markup"] = "<p>" . l("+ Create new protected path", "admin/config/stanford/access-control/new", array("attributes" => array("class" => "btn"))) . "</p>";

  $form['rules_list']['pages_table'] = array(
    '#markup' => stanford_access_control_get_pages_list(),
  );

  return $form;
}


/**
 * Callback to generate list of protected pages.
 */
function stanford_access_control_get_pages_list() {

  $header = array(
    array(
      'data' => t('#'),
    ),
    array(
      'data' => t('Protected Path(s)'),
    ),
    array(
      'data' => t('Enabled / Disabled'),
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
    ->limit(20)
    ->orderByHeader($header);

  $result = $query->execute();

  $rows = array();
  $count = 1;
  foreach ($result as $data) {
    $rows[] = array(
      'data' =>
      array(
        $count,
        $data->path,
        ($data->enabled) ? t("Enabled") : t("Disabled"),
        l(t('Edit'), "admin/config/stanford/access-control/" . $data->pid . "/edit"),
        l(t('Delete'), 'admin/config/stanford/access-control/' . $data->pid . "/delete"),
      ),
    );
    $count++;
  }

  $output = theme('table',
    array(
      'header' => $header,
      'rows' => $rows,
      "sticky" => TRUE,
      "caption" => "",
      "colgroups" => array(),
      "empty" => t("No record found!"),
    )
  );

  $output .= theme('pager', array('tags' => array()));
  return $output;
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
    'users' => array(), // for later when we can get this to work.
    'roles' => $values["roles"],
    'enabled' => $values["enabled"],
    'editor' => $user->uid,
    'changed' => time(),
  );

  // If we are creating a new entry.
  if (isset($values["is_new"]) && $values["is_new"]) {

    // For the log.
    $log_type = "create";

    // Add some more information.
    $data["owner"] = $user->uid;
    $data["created"] = time();

    // Write to the db.
    drupal_write_record("stanford_access_control", $data);
  }
  // If we are updating an entry.
  else {
    // Must have pid as key.
    $keys = array("pid");
    $data["pid"] = $values["pid"];
    drupal_write_record("stanford_access_control", $data, $keys);
  }

  // Let the user know everything is ok.
  drupal_set_message('The settings has been successfully saved.', 'status');

  // Log the change.
  stanford_access_control_log_event($log_type, $values);

  // Must rebuild node_access table...
  node_access_rebuild();

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
      $link = l("See page", "admin/config/stanford/access-control/" . $data["pid"] . "/edit");
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
      $link = l("See page", "admin/config/stanford/access-control/" . $data["pid"] . "/edit");
      break;

    case 'enable':
      $message = "!user enabled protection: @title";
      $variables = array(
        '!user' => l($user->name, "user/" . $user->uid),
        '@title' => $data["admin_title"],
      );
      $link = l("See page", "admin/config/stanford/access-control/" . $data["pid"] . "/edit");
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
    '#type' => 'hidden',
    '#value' => $pid,
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

  // Die if not confirmed.
  if (!$form_state['values']['confirm']) {
    return;
  }

  // Pid from the form submission.
  $pid = $form_state['values']['pid'];

  // Do the delete.
  db_delete('stanford_access_control')
    ->condition('pid', $pid)
    ->execute();

  // Must rebuild node_access table...
  node_access_rebuild();

  // Let the user know stuff happened.
  drupal_set_message(t('The path has been successfully deleted from the protected pages.'));

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


