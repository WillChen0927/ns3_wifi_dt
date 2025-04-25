#!/bin/bash

#initialize globle variables
CURRENT_DIR=$PWD
ROOT_DIR=$CURRENT_DIR/../../

#list of 3gpp yang models
declare -a YANG_MODEL_3GPP=( "_3gpp-common-yang-types.yang"
                        "_3gpp-common-top.yang"
                        "_3gpp-common-measurements.yang"
                        "_3gpp-common-trace.yang"
                        "_3gpp-common-managed-function.yang"
                        "_3gpp-common-subscription-control.yang"
                        "_3gpp-common-fm.yang"
                        "_3gpp-common-managed-element.yang"
                        "_3gpp-5g-common-yang-types.yang"
                        "_3gpp-nr-nrm-rrmpolicy.yang"
                        "_3gpp-nr-nrm-gnbdufunction.yang"
                        "_3gpp-nr-nrm-nrcelldu.yang")


#install 3GPP and ORAN yang modules
installYang()
{
   echo "### install yang modules ###"
   #install 3GPP yang modules
   for yang in "${YANG_MODEL_3GPP[@]}"
   do
      sysrepoctl -i      $ROOT_DIR/build/yang_files/$yang
   done
}

installYang