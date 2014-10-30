#!/bin/ksh -p
#
# CDDL HEADER START
#
# The contents of this file are subject to the terms of the
# Common Development and Distribution License (the "License").
# You may not use this file except in compliance with the License.
#
# You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
# or http://www.opensolaris.org/os/licensing.
# See the License for the specific language governing permissions
# and limitations under the License.
#
# When distributing Covered Code, include this CDDL HEADER in each
# file and include the License file at usr/src/OPENSOLARIS.LICENSE.
# If applicable, add the following below this CDDL HEADER, with the
# fields enclosed by brackets "[]" replaced with your own identifying
# information: Portions Copyright [yyyy] [name of copyright owner]
#
# CDDL HEADER END
#

#
# Copyright 2014 Nexenta Systems, Inc.  All rights reserved.
#

# README
# The following packages must be installed with the indicated syntax to properly run the test suite.
# This installation must either be done as root or with the sudo command:
#    ADD INSTRUCTIONS HERE.	 
#
# Following is the definition of how to run the script and how to update when more tests are added.
# You'll need to run this as sudo without any password needed for all the tests to work properly.
#
# nightly_test [-s <suite name>] [-t <test_name] 
# Where:
#	-s suite_name	Specify the test suite to run. Valid suites are smoke and the test suites. 
#			etc. A full list of the test suites that are supported can be found later
#			in this document by looking at the available_test_suites array.  
#			The tests that make up the smoke tests are defined in the smoke_subtests array.
#			The default is to run all test suites.
#
#	-t test_name	Specify the test name within the test suite. This can be of the form
#			test_name or test_name:test_number. i.e. delete, delete:2, sharemgr/create,
#			or sharemgr/create:1. 
#
#	-a		Autodetect disks for the zfs tests. If this is not specified you must set
#			the disks to be used in the $DISKS environment variable.
#
#	-l		List the valid suite names to pass with the -s option.
#
# Returns: 0 on successful completion, 1 if error.
# 
# When adding a new test suite you need to do the following:
#   1) add the name of the suite here to available_test_suites
#   2) add the package that the suite belongs to to available_test_packages
#   3) add the name of the wrapper script to available_test_wrapper
#   4) (optional) if any of the tests in the suite are to be part of the smoke
#       tests, add the test to smoke_subtests
#   5) (optional) if any of the tests in the suite are to be part of the smoke
#       tests, add the test and it's suite name to smoke_test_suite
set -A available_test_suites lofi-tests sharefs-tests

# Associative array to associate the name of the suite with the package name. This is
# the name that you will see in the /opt directory. 
typeset -A available_test_packages
available_test_packages[lofi-tests]=lofi-tests
available_test_packages[sharefs-tests]=sharefs-tests

# Associative array to associate the name of the suite with the wrapper to run the test. 
# This is the name of the binary located in /opt/<package name>/bin.
typeset -A available_test_wrapper
available_test_wrapper[lofi-tests]=lofitest
available_test_wrapper[sharefs-tests]=sharetest

# The following arrays define the tests that make up the smoke tests. 
# The smoke_subtests define the specific test to be run. It can be a
# set of tests like compression_static or it can be a specific test like
# compression_static:2. When adding a new test to the smoke tests you'll
# need to add it here and which suite of tests in the following smoke_test_suite
# associative array.
set -A smoke_subtests "compression_static:2" "compression_static:4" delete

# The associative array that associates the tests with the suite they reside in.
typeset -A smoke_test_suite
smoke_test_suite[compression_static:2]=lofi-tests
smoke_test_suite[compression_static:4]=lofi-tests
smoke_test_suite[delete]=lofi-tests

OPTHOME=/opt
TMPDIR=/var/tmp

sudo -n id >/dev/null 2>&1
[[ $? -eq 0 ]] || exit 1 "User must be able to sudo without a password."

#
# Create mail_msg_file
#
TMPDIR="$TMPDIR"
TEST_RESULTS_DIR=${TMPDIR}/test_results
/usr/bin/mkdir -p ${TEST_RESULTS_DIR}
mail_msg_file="${TEST_RESULTS_DIR}/nightly_test_msg"

integer prev_total_tests=0
integer prev_total_pass=0
integer prev_total_fail=0
integer prev_total_uninitiated=0
integer prev_total_unresolved=0
integer prev_total_other=0
integer baseline_total_tests=0
integer baseline_total_pass=0
integer baseline_total_fail=0
integer baseline_total_uninitiated=0
integer baseline_total_unresolved=0
integer baseline_total_other=0
integer total_tests=0
integer total_pass=0
integer total_fail=0
integer total_uninitiated=0
integer total_unresolved=0
integer total_other=0

#
# function to run the wrapper script for the specified test. 
#    $1 : Test suite to run. This is something like lofi-tests. It is required.
#    $2 : Optional to specify a specific test within a suite to run. This is something like
#	  delete or delete:2
#
function run_suite {
	if [ -f ${TEST_RESULTS_DIR}/$1/testlog ] ; then
		rm ${TEST_RESULTS_DIR}/$1/testlog
	fi

	# See if the caller has supplied a specific test or wants to run the entire suite.
	if [ $# -eq 2 ] ; then
		subtest_name=$(echo "$2" | sed "s/\//./g")
	fi

	if [ $# -eq 2 ] ; then
		if [ -f ${TEST_RESULTS_DIR}/$1/$subtest_name.txt ] ; then
			sudo mv ${TEST_RESULTS_DIR}/$1/$subtest_name.txt ${TEST_RESULTS_DIR}/$1/$subtest_name.prev
		fi
		# execute wrapper with a specific test to run 
		if [ -f $OPTHOME/${available_test_packages[$1]}/bin/${available_test_wrapper[$1]} ] ; then 
			if [ $1 == "zfs-tests" and $auto_detect == "yes" ] ; then
				echo $OPTHOME/${available_test_packages[$1]}/bin/${available_test_wrapper[$1]} -a $2
				$OPTHOME/${available_test_packages[$1]}/bin/${available_test_wrapper[$1]} -a $2
			else
				sudo $OPTHOME/${available_test_packages[$1]}/bin/${available_test_wrapper[$1]}  $2
			fi
		else
			echo $OPTHOME/${available_test_packages[$1]}/bin/${available_test_wrapper[$1]} does not exist. Was ${available_test_packages[$1]} installed properly? 
			exit 1
		fi
		sudo mv ${TEST_RESULTS_DIR}/$1/summary.txt ${TEST_RESULTS_DIR}/$1/$subtest_name.txt
	else
		if [ -f ${TEST_RESULTS_DIR}/$1/summary.txt ] ; then
			sudo mv ${TEST_RESULTS_DIR}/$1/summary.txt ${TEST_RESULTS_DIR}/$1/summary.prev
		fi
		# execute wrapper to run entire suite
		if [ -f $OPTHOME/${available_test_packages[$1]}/bin/${available_test_wrapper[$1]} ] ; then 
			if [ $1 == "zfs-tests" and $auto_detect == "yes" ] ; then
				$OPTHOME/${available_test_packages[$1]}/bin/${available_test_wrapper[$1]} -a	
			else
				sudo $OPTHOME/${available_test_packages[$1]}/bin/${available_test_wrapper[$1]}	
			fi
		else
			echo $OPTHOME/${available_test_packages[$1]}/bin/${available_test_wrapper[$1]} does not exist. Was ${available_test_packages[$1]} installed properly? 
			exit 1
		fi
	fi
	return
}

#
# run the predefined smoke tests. Takes no arguments
#
function run_smoketests {
	for subtest in ${smoke_subtests[@]}; do
		# Copy results from previous run from <test name>.txt to <test name>.prev
		if [ -f ${TEST_RESULTS_DIR}/${smoke_test_suite[$subtest]}/$subtest.txt ] ; then
   	    		sudo mv ${TEST_RESULTS_DIR}/${smoke_test_suite[$subtest]}/$subtest.txt\
	      		   ${TEST_RESULTS_DIR}/${smoke_test_suite[$subtest]}/$subtest.prev
		fi
		# Run the test
		run_suite ${smoke_test_suite[$subtest]} $subtest
		# The results are stored in summary.txt. Copy this to <test name>.txt
		if [ -f ${TEST_RESULTS_DIR}/${smoke_test_suite[$subtest]}/summary.txt ] ; then
	    		sudo mv ${TEST_RESULTS_DIR}/${smoke_test_suite[$subtest]}/summary.txt\
	       		   ${TEST_RESULTS_DIR}/${smoke_test_suite[$subtest]}/$subtest.txt
		fi
    	done
	return
}

#
# Append the difference between the previous run and this run to the email message
# $1 : Full path to the test results for the suite that was run.
# $2 : The base name of the file where the tests results are stored.
#
function prev_diff_to_email {
	if [ -f $1/$2.prev ] ; then
		diff $1/$2.prev $1/$2.txt >> $mail_msg_file
	fi
	return
}

#
# append the diff between the baseline run and this run to the email message
# $1 : The suite that was run.
# $2 : The name of the baseline test file 
# $3 : The name of the file where the test results are stored
#
function baseline_diff_to_email {
	if [ -f $OPTHOME/$1/baseline/$2 ] ; then
		diff $OPTHOME/$1/baseline/$2 \
			${TEST_RESULTS_DIR}/$1/$3 >> $mail_msg_file
	else
		echo $OPTHOME/$1/baseline/$2 does not exist. Was ${available_test_packages[$1]} installed properly? 
		exit 1
	fi
        return
}

#
# Find the number of tests in the specified summary file that passed
# $1 : Full path to summary file
# return : number of PASS tests
# 
function detect_num_pass  {
	pass=`grep "PASS" $1 | grep ":" | awk '{ print $3 }'`
}

#
# Find the number of tests in the specified summary file that failed 
# $1 : Full path to summary file
# return : number of FAIL tests
# 
function detect_num_fail {
	fail=`grep "FAIL" $1 | grep ":" | awk '{ print $3 }'`
}

#
# Find the number of tests in the specified summary file that were uninitiated 
# $1 : Full path to summary file
# return : number of UNINITIATED tests
# 
function detect_num_uninitiated {
	uninitiated=`grep "UNINITIATED" $1 | grep ":" | awk '{ print $3 }'`
}

#
# Find the number of tests in the specified summary file that were unresolved 
# $1 : Full path to summary file
# return : number of  UNRESOLVED tests
# 
function detect_num_unresolved {
	unresolved=`grep "UNRESOLVED" $1 | grep ":" | awk '{ print $3 }'`
}

#
# Find the number of tests in the specified summary file that were declared
# to be OTHER status  
# $1 : Full path to summary file
# return : number of OTHER tests
# 
function detect_num_other {
	other=`grep "OTHER" $1 | grep ":" | awk '{ print $3 }'`
}

#
# Gather the statistics for the current run. Later the values will be printed to the email
# file
# $1 : Full path to file to gather the statistics from
#
function gather_current_statistics {
	# Look for PASS : in the test results summary file for this suite. Add the
	# number of passes to our running total.
	detect_num_pass $1
	total_pass+=$pass
	total_tests+=$pass

	# Look for FAIL : in the test results summary file for this suite. Add the
	# number of failures to our running total.
	detect_num_fail $1
	total_fail+=$fail
	total_tests+=$fail

	# Look for UNINITIATED : in the test results summary file for this suite. Add the
	# number of uninitiated tests to our running total.
	detect_num_uninitiated $1
	total_uninitiated+=$uninitiated
	total_tests+=$uninitiated 

	# Look for UNRESOLVED : in the test results summary file for this suite. Add the
	# number of unresolved tests to our running total.
	detect_num_unresolved $1
	total_unresolved+=$unresolved
	total_tests+=$unresolved

	# Look for OTHER : in the test results summary file for this suite. Add the
	# number of other status tests to our running total.
	detect_num_other $1
	total_other+=$other
	total_tests+=$other

	return
}

#
# Gather the statistics for the previous run. Later the values will be printed to the email
# file
# $1 : Full path to file to gather the statistics from
#
function gather_previous_statistics {
	# Look for PASS : in the previous test results summary file for this suite. Add the
	# number of passes to our running total.
	detect_num_pass $1
	prev_total_pass+=$pass
	prev_total_tests+=$pass

	# Look for FAIL : in the previous test results summary file for this suite. Add the
	# number of failures to our running total.
	detect_num_fail $1
	prev_total_fail+=$fail
	prev_total_tests+=$fail

	# Look for UNINITIATED : in the previous test results summary file for this suite. Add the
	# number of uninitiated tests to our running total.
	detect_num_uninitiated $1
	prev_total_uninitiated+=$uninitiated
	prev_total_tests+=$uninitiated 

	# Look for UNRESOLVED : in the previous test results summary file for this suite. Add the
	# number of unresolved tests to our running total.
	detect_num_unresolved $1
	prev_total_unresolved+=$unresolved
	prev_total_tests+=$unresolved

	# Look for OTHER : in the previous test results summary file for this suite. Add the
	# number of other status tests to our running total.
	detect_num_other $1
	prev_total_other+=$other
	prev_total_tests+=$other
	return
}

#
# Gather the statistics for the baseline run. Later the values will be printed to the email
# file
# $1 : Full path to file to gather the statistics from
#
function gather_baseline_statistics {
	# Look for PASS : in the baseline summary file for this suite. Add the
	# number of passed tests to our running total.
	detect_num_pass $1
	baseline_total_pass+=$pass
	baseline_total_tests+=$pass

	# Look for FAIL : in the baseline summary file for this suite. Add the
	# number of failed tests to our running total.
	detect_num_fail $1
	baseline_total_fail+=$fail
	baseline_total_tests+=$fail

	# Look for UNINITIATED : in the baseline summary file for this suite. Add the
	# number of uninitiated tests to our running total.
	detect_num_uninitiated $1
	baseline_total_uninitiated+=$uninitiated
	baseline_total_tests+=$uninitiated 

	# Look for UNRESOLVED : in the baseline summary file for this suite. Add the
	# number of unresolved tests to our running total.
	detect_num_unresolved $1
	baseline_total_unresolved+=$unresolved
	baseline_total_tests+=$unresolved

	# Look for OTHER : in the baseline summary file for this suite. Add the
	# number of other status tests to our running total.
	detect_num_other $1
	baseline_total_other+=$other
	baseline_total_tests+=$other

	return
}

# standard outputs for the email message
function previous_diffs_header {
	echo "\n==== Test run differences from previous run ====" >> $mail_msg_file
	return
}

function baseline_diffs_header {
	echo "\n==== Test run differences from baseline run ====" >> $mail_msg_file
	return
}

function summary_failed_tests_header {
	echo "\n=========Summary for Failed test runs ==========" >> $mail_msg_file 
	return
}

function details_failed_tests_header {
	echo "\n=========Details for Failed test runs ==========" >> $mail_msg_file 
	return
}

function previous_diffs_output {
	echo "\n"The previous run ran $prev_total_tests tests, $prev_total_pass passed, $prev_total_fail failed, $prev_total_uninitiated were uninitiated, $prev_total_unresolved were unresolved, and $prev_total_other had other results. >> $mail_msg_file
	return
}

function current_diffs_output {
	echo "\n"The current run ran $total_tests tests, $total_pass passed, $total_fail failed, $total_uninitiated were uninitiated, $total_unresolved were unresolved, and $total_other had other results. >> $mail_msg_file
	return
}

function baseline_diffs_output {
	echo "\n"The baseline run ran $baseline_total_tests tests, $baseline_total_pass passed, $baseline_total_fail failed, $baseline_total_uninitiated were uninitiated, $baseline_total_unresolved were unresolved, and $baseline_total_other had other results. >> $mail_msg_file
	return
}

function summary_failed_tests_output {
	grep FAIL $1 | grep -v ":" >> $mail_msg_file
}

#
# Output the details of each of the failed tests to the email message
# $1 : test suite name
# $2 : Full path name for the summary file for this test.
#
function details_failed_tests_output {
	grep FAIL $2 | grep -v ":" > tmp
	file="tmp"
	while read tp test_num test_name status; do
		fail_file=$test_name"."$test_num
		echo "\n\n"$fail_file >> $mail_msg_file
		test_name=$(echo "$test_name" | sed "s/tc_//")
		if [ -d $OPTHOME/$1/runfiles ] ; then
			# These are suites run by test-runner so the results are in a somewhat
			# different location
			cat ${TEST_RESULTS_DIR}/$1/*/$test_name/stdout >> $mail_msg_file
		else
			cat ${TEST_RESULTS_DIR}/$1/sorted_results/$test_name/FAIL/$fail_file >> $mail_msg_file
		fi
	done <"$file"
	rm tmp

	return
}

#
# Place the results of the smoke test run into the email message.
#
function generate_smoke_test_email {
	prev_total_tests=0
	prev_total_pass=0
	prev_total_fail=0
	prev_total_uninitiated=0
	prev_total_unresolved=0
	prev_total_other=0
	baseline_total_tests=0
	baseline_total_pass=0
	baseline_total_fail=0
	baseline_total_uninitiated=0
	baseline_total_unresolved=0
	baseline_total_other=0
	total_tests=0
	total_pass=0
	total_fail=0
	total_uninitiated=0
	total_unresolved=0
	total_other=0

	#
	# First item in the email is differences between the current run and the
	# previous run followed by statistics for both runs.
	previous_diffs_header
	for subtest in ${smoke_subtests[@]}; do
		prev_diff_to_email ${TEST_RESULTS_DIR}/${smoke_test_suite[$subtest]} $subtest
		gather_current_statistics ${TEST_RESULTS_DIR}/${smoke_test_suite[$subtest]}/$subtest.txt
	done
	if [ -f ${TEST_RESULTS_DIR}/${smoke_test_suite[$subtest]}/$subtest.prev ] ; then
		for subtest in ${smoke_subtests[@]}; do
			gather_previous_statistics ${TEST_RESULTS_DIR}/${smoke_test_suite[$subtest]}/$subtest.prev
		done

		previous_diffs_output
		current_diffs_output
	fi
	baseline_diffs_header
	for subtest_name in ${smoke_subtests[@]}; do
		subtest=$(echo $subtest_name | sed "s/:/./g")
                baseline_diff_to_email ${smoke_test_suite[$subtest_name]} ${subtest}.txt ${subtest_name}.txt
		gather_baseline_statistics $OPTHOME/${smoke_test_suite[$subtest_name]}/baseline/${subtest}.txt
        done

	baseline_diffs_output
	current_diffs_output

	summary_failed_tests_header
	for subtest in ${smoke_subtests[@]}; do
		summary_failed_tests_output ${TEST_RESULTS_DIR}/${smoke_test_suite[$subtest]}/${subtest}.txt
	done
	details_failed_tests_header
	for subtest in ${smoke_subtests[@]}; do
		details_failed_tests_output ${smoke_test_suite[$subtest]} ${TEST_RESULTS_DIR}/${smoke_test_suite[$subtest]}/${subtest}.txt  
	done
	return
}

#
# Place the results of run with the suite/specific tests specified into the email message.
# $1 : name of the suite
# $2 : (optional) name of the specific test in the suite. Can be of the form delete or delete:2
#
function generate_suite_email {
	prev_total_tests=0
	prev_total_pass=0
	prev_total_fail=0
	prev_total_uninitiated=0
	prev_total_unresolved=0
	prev_total_other=0
	baseline_total_tests=0
	baseline_total_pass=0
	baseline_total_fail=0
	baseline_total_uninitiated=0
	baseline_total_unresolved=0
	baseline_total_other=0
	total_tests=0
	total_pass=0
	total_fail=0
	total_uninitiated=0
	total_unresolved=0
	total_other=0

	previous_diffs_header

	if [ $# -eq 2 ] ; then
		subtest_name=$(echo "$2" | sed "s/\//./g")
		prev_diff_to_email ${TEST_RESULTS_DIR}/$1 $subtest_name
		gather_current_statistics ${TEST_RESULTS_DIR}/$1/$subtest_name.txt
		if [ -f ${TEST_RESULTS_DIR}/$1/$subtest_name.prev ] ; then
			gather_previous_statistics ${TEST_RESULTS_DIR}/$1/$subtest_name.prev
			previous_diffs_output
			current_diffs_output
		fi
	else
		prev_diff_to_email ${TEST_RESULTS_DIR}/$1 summary 
		gather_current_statistics ${TEST_RESULTS_DIR}/$1/summary.txt
		if [ -f ${TEST_RESULTS_DIR}/$1/summary.prev ] ; then
			gather_previous_statistics ${TEST_RESULTS_DIR}/$1/summary.prev
			previous_diffs_output
			current_diffs_output
		fi
	fi

	baseline_diffs_header

	if [ $# -eq 2 ] ; then
		_subtest_name=$(echo $subtest_name | sed "s/:/./g")
       		baseline_diff_to_email $1 $_subtest_name.txt $subtest_name.txt
		gather_baseline_statistics $OPTHOME/$1/baseline/$_subtest_name.txt
       	else
              	baseline_diff_to_email $1 summary.txt summary.txt
		gather_baseline_statistics $OPTHOME/$1/baseline/summary.txt
	fi

	baseline_diffs_output
	current_diffs_output
	summary_failed_tests_header
	if [ $# -eq 2 ] ; then
		summary_failed_tests_output ${TEST_RESULTS_DIR}/$1/$subtest_name.txt
	else
		summary_failed_tests_output ${TEST_RESULTS_DIR}/$1/summary.txt
	fi
	details_failed_tests_header
	if [ $# -eq 2 ] ; then
		details_failed_tests_output $1 ${TEST_RESULTS_DIR}/$1/$subtest_name.txt
	else
		details_failed_tests_output $1 ${TEST_RESULTS_DIR}/$1/summary.txt
	fi
	return
}

#
# Place the results of the "all tests" test run into the email message.
#
function generate_all_test_email {
	prev_total_tests=0
	prev_total_pass=0
	prev_total_fail=0
	prev_total_uninitiated=0
	prev_total_unresolved=0
	prev_total_other=0
	baseline_total_tests=0
	baseline_total_pass=0
	baseline_total_fail=0
	baseline_total_uninitiated=0
	baseline_total_unresolved=0
	baseline_total_other=0
	total_tests=0
	total_pass=0
	total_fail=0
	total_uninitiated=0
	total_unresolved=0
	total_other=0

	#
	# First item in the email is differences between the current run and the
	# previous run followed by statistics for both runs.
	# We'll need to cycle through each of the test suites
	previous_diffs_header
	for suite in ${available_test_suites[@]}; do
		# Detect the differences and send them to the email message file
		prev_diff_to_email ${TEST_RESULTS_DIR}/${available_test_packages[$suite]} summary 

		# gather statistics from the current run
		gather_current_statistics ${TEST_RESULTS_DIR}/$suite/summary.txt
	done

	#
	# If there has been a previous test run, print out the statistics. Otherwise we
	# don't have anything to compare to so don't bother.
	#
	if [ -f ${TEST_RESULTS_DIR}/summary.prev ] ; then
		gather_previous_statistics ${TEST_RESULTS_DIR}/summary.prev

		previous_diffs_output
		current_diffs_output
	fi

	#
	# Create a top level summary.prev with the results so we have something to compare to
	# next time we do an "all tests" run.
	# 
	if [ -f ${TEST_RESULTS_DIR}/summary.prev ] ; then
		rm ${TEST_RESULTS_DIR}/summary.prev
	fi
	touch ${TEST_RESULTS_DIR}/summary.prev
	echo "\nPASS : " $total_pass >> ${TEST_RESULTS_DIR}/summary.prev
	echo "\nFAIL : " $total_fail >> ${TEST_RESULTS_DIR}/summary.prev
	echo "\nUNINITIATED : " $total_uninitiated >> ${TEST_RESULTS_DIR}/summary.prev
	echo "\nUNRESOLVED : " $total_unresolved >> ${TEST_RESULTS_DIR}/summary.prev
	echo "\nOTHER : " $total_other >> ${TEST_RESULTS_DIR}/summary.prev

	#
	# The next section in the email message is differences between the current
	# test run and the baseline run.
	#
	baseline_diffs_header

	#
	# For each suite, perform a diff between the baseline summary.txt file located
	# in /opt/<suite package>/baseline/summary.txt and the current run's
	# summary.txt file located in /var/tmp/test_results/<suite>/summary.txt. 
	# Then gather the statistics for the baseline run. Output both baseline and
	# current run statistics to the email file.
	for suite in ${available_test_suites[@]}; do
        	baseline_diff_to_email $suite summary.txt summary.txt

		gather_baseline_statistics $OPTHOME/$suite/baseline/summary.txt
	done

	baseline_diffs_output
	current_diffs_output

	summary_failed_tests_header
	for suite in ${available_test_suites[@]}; do
		summary_failed_tests_output ${TEST_RESULTS_DIR}/$suite/summary.txt
	done
	details_failed_tests_header
	for suite in ${available_test_suites[@]}; do
		details_failed_tests_output $suite ${TEST_RESULTS_DIR}/$suite/summary.txt  
	done
	return
}

#
# Will generate a string that lists the possible input to the -t option for a given test suite directory.
# It will place the directories and the specific test into the message. i.e. sharemgr sharemgr/add sharemgr/add:1
# sharemgr/add:2 into the list.
#
# $1: List of files/directories in a directory to be scanned
# $2: Name of the calling directory
# returns: name of a directory to add to the message. It will also modify the message (list_msg)   
function find_tests {
	found_test=""
	typeset -i count
	count=0
	for file in $1 ; do
		if [ -d $file ] ; then
			find_tests "$file/*" $file
			list_msg="\n\n$found_test  "$list_msg
			if [ found_test!="" ] ; then
				found_test=$2
			fi
		else
			case "$file" in
				*tp_*)  if [ $count == 0 ] ; then
					        found_test="$2\n"
				        fi
				        count=$count+1
			       		list_msg="    $2:$count"$list_msg ;;
				*);;
			esac
		fi
	done	
	return $found_test
}

USAGE='Usage: nightly_test [-s <suite name>] [-t <test_name] [-a] [-l]\n 

Where: \n
	\t-s suite_name\tSpecify the test suite to run. Valid suites can be obtained by using the -l option.\n\t\t\tThe default is to run all test suites.\n\n

	\t-t test_name\tSpecify the test name within the test suite.\n
	\t\t\tThis can be of the form test_name or test_name:test_number. i.e. delete or delete:2.\n\n

	\t-a\t\tAutomatically detect the disks to be used for the zfs tests. \n\t\t\tDefault is to have the user specify the disks in $DISKS.\n\n

	\t-l\t\tList all valid test suites. If used in conjunction with the -s option, it will \n\t\t\tlist all tests within the specified suite.	
'
#
# examine arguments
#

test_all=0
test_suite="xxx"
test_subsuite="yyy"
auto_detect="no"
list="no"

if [[ $# -eq 0 ]]; then
	# specific suite not supplied to execute all test suites.
	test_all=1	
else
	while getopts als:t: optarg
	do	case "$optarg" in
		a) auto_detect="yes";;
		l) list="yes";;
		s) test_suite="$OPTARG"
			if [ $test_suite != "smoke" ] ; then
				valid_suite=no
				for suite in ${available_test_suites[@]} ; do
					if [ $test_suite == $suite ] ; then
						valid_suite=yes
					fi
				done
				if [ $valid_suite != "yes" ] ; then
					echo Invalid suite specified.
					echo $USAGE
					exit 1
				fi
			fi
			;;		
		t) test_subsuite="$OPTARG";;
		[?]) echo $USAGE
			exit 1;;
		esac
	done
	shift $OPTIND-1
fi

if [ $list == "yes" ] ; then
	if [ $test_suite == "xxx" ] ; then
		# -l option with no suite. Print out which suites the user can specify
		list_msg="Valid suites names to use with the -s option are: smoke"
		for suite in ${available_test_suites[@]}; do
			list_msg=$list_msg", $suite"
		done
	elif [ $test_suite == "smoke" ] ; then
		for subtest in ${smoke_subtests[@]}; do
			list_msg=$list_msg"\n"${smoke_test_suite[$subtest]}/$subtest
		done
	else
		# -l option with suite specified. Print out which tests withing the suite
		# the user can specify
		cd $OPTHOME/${available_test_packages[$test_suite]}/tests
		list_msg=""
		find_tests "*"
	fi
	echo $list_msg
	exit 0
fi

#
# Check to make sure that all of the packages we need to run the
# complete test suites have been installed. If not, exit with
# an error message.
#
if [ ! -d /opt/SUNWstc-dtet ] ; then
	echo "The SUNWstc-dtet package is not installed.\n"\
	     "Please download and install this package." 
fi

if [ ! -d /opt/SUNWstc-tetlite ] ; then
	echo "The SUNWstc-tetlite package is not installed.\n"\
	     "Please download and install this package." 
fi

if [ ! -d /opt/SUNWstc-genutils ] ; then
	echo "The SUNWstc-genutils package is not installed.\n"\
	     "Please download and install this package." 
fi

if [ ! -d /opt/SUNWstc-stf ] ; then
	echo "The SUNWstc-stf package is not installed.\n"\
	     "Please download and install this package." 
fi

#
# Check to make sure that if the user will be running the zfs test suite via
# running all the suites, specifying it with the -s option, or running a 
# smoke test that is in the zfs suite, that they have either specified to
# autodetect he disks to use via the -a option or have set $DISKS
#
if [ $test_all == 1 ] ; then
	for suite in ${available_test_suites[@]}; do
		if [ $suite == "zfs-tests" ] ; then
			if [ $auto_detect != "yes" ] ; then
				if [[ -z $DISKS ]]; then
        				echo "\$DISKS not set in env, and -a not specified."
				fi
				exit 1
			fi
		fi
	done
elif [ $test_suite == "zfs-tests" ] ; then
	if [ $auto_detect != "yes" ] ; then
		if [[ -z $DISKS ]]; then
       			echo "\$DISKS not set in env, and -a not specified."
		fi
		exit 1
	fi
elif [ $test_suite == "smoke" ] ; then
	for subtest in ${smoke_subtests[@]}; do
		if [ ${smoke_test_suite[$subtest]} == "zfs-tests" ] ; then
			if [ $auto_detect != "yes" ] ; then
				if [[ -z $DISKS ]]; then
        				echo "\$DISKS not set in env, and -a not specified."
				fi
				exit 1
			fi
		fi
	done
fi 

#
# Remove the old mail message to be replaced with the current one.
if [ -f $mail_msg_file ] ; then
	rm $mail_msg_file
fi
/usr/bin/touch $mail_msg_file

#
# If the user has specified to run the entire test suite, just cycle through
# the list of suites executing each one.
#
if [ $test_all -eq 1 ] ; then
	for suite in ${available_test_suites[@]}; do
		run_suite $suite
	done


	#
	# Now generate the nightly_test email message.
	#
	generate_all_test_email
else
	#
	# If the user has specified to run the smoke tests, we run a predetermined set
	# of tests defined in the array smoke_subtests.
	#
	if [ $test_suite = "smoke" ] ; then
		run_smoketests

		#
		# Generate the nightly_test email
		#
		generate_smoke_test_email
		exit 0
	else
		if [ $test_suite = "xxx" ] ; then
			echo $USAGE
			exit 1
		fi
		#
		# The user has specified a suite to run via the -s option. If the user has
		# also specified a subsuite, run only the subsuite within the specified suite.
		#
		if [ $test_subsuite != "yyy" ] ; then
                        run_suite $test_suite $test_subsuite
			generate_suite_email $test_suite $test_subsuite
                else
                        run_suite $test_suite
			generate_suite_email $test_suite
                fi
	fi

fi

echo Summary of the test results is located at $mail_msg_file
exit 0 
