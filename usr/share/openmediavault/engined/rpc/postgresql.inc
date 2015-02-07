<?php

/**
 * Copyright (C) 2010-2012 Ian Moore <imooreyahoo@gmail.com>
 * Copyright (C) 2013-2015 OpenMediaVault Plugin Developers
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

require_once "openmediavault/config.inc";
require_once "openmediavault/error.inc";
require_once "openmediavault/notify.inc";
require_once "openmediavault/object.inc";
require_once "openmediavault/rpcservice.inc";
require_once "openmediavault/util.inc";

class OMVRpcServicePostgreSQL extends OMVRpcServiceAbstract
{
    private function getEventMessagePath()
    {
        return "org.openmediavault.services.postgresql";
    }

    private function getXPath()
    {
        return "//services/postgresql";
    }

    public function getName()
    {
        return "PostgreSQL";
    }

    public function initialize()
    {
        $this->registerMethod("getSettings");
        $this->registerMethod("setSettings");
        $this->registerMethod("resetPassword");
    }

    /**
     * Get main settings.
     */
    public function getSettings($params, $context)
    {
        global $xmlConfig;

        // Validate the RPC caller context.
        $this->validateMethodContext($context, array(
            "role" => OMV_ROLE_ADMINISTRATOR
        ));

        // Get configuration object
        $object = $xmlConfig->get($this->getXPath());

        if (is_null($object)) {
            throw new OMVException(
                OMVErrorMsg::E_CONFIG_GET_OBJECT_FAILED,
                $this->getXPath()
            );
        }

        // Get actual postgresql status
        $initd = new OMVSysVInitScript("postgresql");

        // Modify result data
        $object["enable"]            = intval($object["enable"]);
        $object["port"]              = intval($object["port"]);

        $object["enable_management_site"] = boolval($object["enable_management_site"]);
        $object["show_tab"]               = boolval($object["show_tab"]);

        return $object;
    }

    /**
     * Set main settings.
     */
    public function setSettings($params, $context)
    {
        global $xmlConfig;

        // Validate the RPC caller context.
        $this->validateMethodContext($context, array(
            "role" => OMV_ROLE_ADMINISTRATOR
        ));

        // Validate the parameters of the RPC service method.
        $this->validateMethodParams(
            $params,
            '{
                "type"       : "object",
                "properties" : {
                    "enable"               : { "type" : "boolean" },
                    "port"                 : { "type" : "integer" },
                    "bind_address"         : { "type" : "string" },
                    "data.sharedfolderref" : {' . $GLOBALS["OMV_JSONSCHEMA_UUID_NONE"] . '},
                    "extra_options"        : { "type" : "string" },

                    "enable_management_site" : { "type" : "boolean" },
                    "show_tab"               : { "type" : "boolean", "optional" : true }
                }
            }'
        );

        // Prepare configuration data
        $object = array(
            "enable"               => array_boolval($params, "enable"),
            "port"                 => intval($params["port"]),
            "bind_address"         => $params["bind_address"],
            "data.sharedfolderref" => $params["data.sharedfolderref"],
            "extra_options"        => trim($params["extra_options"]),

            "enable_management_site" => (
                array_boolval($params, "enable_management_site") &&
                array_boolval($params, "enable")
            ),
            "show_tab" => array_boolval($params, "show_tab"),
        );

        if ($object["enable"] && empty($object["data.sharedfolderref"])) {
            throw new OMVException(
                OMVErrorMsg::E_CONFIG_OBJECT_INVALID,
                "No data directory set when the plugin is enabled."
            );
        }

        // Set configuration object
        if (false === $xmlConfig->replace($this->getXPath(), $object)) {
            throw new OMVException(
                OMVErrorMsg::E_CONFIG_SET_OBJECT_FAILED,
                $this->getXPath()
            );
        }

        // Notify configuration changes
        $dispatcher = &OMVNotifyDispatcher::getInstance();
        $dispatcher->notify(
            OMV_NOTIFY_MODIFY,
            $this->getEventMessagePath(),
            $object
        );
    }

    /**
     * Reset root password.
     */
    public function resetPassword($params, $context)
    {
        global $xmlConfig;

        // Validate the RPC caller context.
        $this->validateMethodContext($context, array(
            "role" => OMV_ROLE_ADMINISTRATOR
        ));

        // Validate the parameters of the RPC service method.
        $this->validateMethodParams(
            $params,
            '{
                "type"       : "object",
                "properties" : {
                    "root_password" : { "type" : "string" }
                }
            }'
        );

        // Create password file
        $file = "/tmp/.openmediavault-postgresql-reset-pwd";
        file_put_contents($file, $params["root_password"]);

        $cmd = "sudo /usr/local/bin/postgresql-password 2>&1";

        if (0 !== $this->exec($cmd, $output)) {
            throw new OMVException(
                OMVErrorMsg::E_EXEC_FAILED,
                $cmd,
                implode("\n", $output)
            );
        }
    }
}

// Register the RPC service.
$rpcServiceMgr = &OMVRpcServiceMgr::getInstance();
$rpcServiceMgr->registerService(new OMVRpcServicePostgreSQL());
