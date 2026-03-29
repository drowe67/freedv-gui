#!/bin/bash

until sqlite3 $HOME/Library/Application\ Support/com.apple.TCC/TCC.db "BEGIN TRANSACTION; INSERT OR IGNORE INTO access VALUES ('kTCCServiceMicrophone','/usr/local/opt/runner/provisioner/provisioner',1,2,4,1,NULL,NULL,0,'UNUSED',NULL,0,1687786159,NULL,NULL,'UNUSED',1687786159); INSERT OR IGNORE INTO access VALUES ('kTCCServiceMicrophone','/opt/off/opt/runner/provisioner/provisioner',1,2,4,1,NULL,NULL,0,'UNUSED',NULL,0,1687786159,NULL,NULL,'UNUSED',1687786159); COMMIT;"; do
    echo "Failed, trying again"
    sleep 1
done
