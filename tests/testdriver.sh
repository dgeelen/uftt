#!/bin/bash

trap handle_exit ERR

COLOR_WARN="\033[01;33m"
COLOR_ERROR="\033[01;31m"
COLOR_STATUS="\033[01;32m"
COLOR_HIGHLIGHT="\033[01;36m"
COLOR_DEBUG="\033[01;35m"
COLOR_DEFAULT="\033[00m"
function message() {
	COLOR_CODE=$(eval echo "\${${1}}")
	echo -e "> ${COLOR_CODE}${2}${COLOR_DEFAULT}" 1>&2
}

function die() {
	message COLOR_ERROR "${1}"
	exit 1
}

function do_kill() {
	if uname | grep -i cygwin; then
		"${SCRIPTDIR}/cygkill.sh" "${1}"
	else
		kill "${1}"
	fi
}

# Find a netcat binary. The original netcat uses 'nc', whereas gnu-netcat
# uses 'ncat' and 'netcat'.
NETCAT="$(which nc ncat netcat 2>/dev/null | head -1)"
if [ -z "${NETCAT}" ] ; then
	die "Error: could not locate a 'netcat' binary. Please install gnu-netcat or similar, then try again."
fi

# strict mode, all commands must have 0 exit code, no unfinished jobs
function handle_exit {
	local EXITCODE=$?
	local JOBSLEFT=0

	if mv "${LOCKFILE}.init" "${LOCKFILE}.done"; then
		do_kill ${TIMEOUTPID} || true
		wait ${TIMEOUTPID} || true
	else
		EXITCODE=2
	fi

	for job in $(jobs -p); do
		echo "Unfinished job: $job"
		JOBSLEFT=1
		kill $job || true
	done

	if [ "x${JOBSLEFT}" == "x1" ]; then
		EXITCODE=1
		sleep 1
		for job in $(jobs -p); do
			echo "Unkilled job: $job"
			kill -9 $job || true
			wait $job || true
		done
	fi

	# Clean up temp location
	if [ "x${EXITCODE}" == "x0" ]; then
		cd ..
		rm -r "${TEMPDIR}"
	fi

	exit $EXITCODE
}


# Start an uftt instance in the background, and wait for it to spin up
# UFTTPID is set to the pid of the started instance
function start_uftt_bg {
	if ("${NETCAT}" -h 2>&1 | grep -o -i bsd); then
		"${NETCAT}" -l 47187&
	else
		"${NETCAT}" -l -p 47187&
	fi
	NCPID=$!

	"${UFTT}" --notify-socket 47187&
	UFTTPID=$!

	wait ${NCPID}
}

# Find script location
SCRIPTDIR="$(cd $(dirname "$0"); pwd)"

# Make temp location
TEMPDIR="$(mktemp -d --tmpdir uftt.test.XXXXXXXXXX)"

# Create lockfile
LOCKFILE="${TEMPDIR}/lockfile"
touch "${LOCKFILE}.init"

# Enter temp location
cd "${TEMPDIR}"

# Create local settings file for tests to use
touch uftt.dat

# Set uftt Location
UFTT="$2"

# Set up timeout (20 seconds)
DRIVERPID=$$
sleep 20 && mv "${LOCKFILE}.init" "${LOCKFILE}.timeout" 2>/dev/null && echo Timeout && do_kill ${DRIVERPID} || true&
TIMEOUTPID=$!

# Execute test script
source "${SCRIPTDIR}/$1"

handle_exit
