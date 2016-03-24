import prlsdkapi
import sys
import time
import os
import subprocess
from prlsdkapi import PrlSDKError, consts

DIR = os.path.dirname(os.path.abspath(__file__))
VMIO_PRESENT = hasattr(prlsdkapi, 'VmIO')

class DoneException(Exception):
	"""
	Raise this exception if all statistics about current VE have been collected
	"""
	pass

class ConnectionError(Exception):
	"""
	Raise this exception if connection related problems have occured
	"""
	pass

class VmGuestSessionCommon(object):
	"""
	Parent class for VE wrappers. This is bootstraps for prlsdkapi
	"""
	def __init__(self, sdk_vm):
		self._sdk_vm = sdk_vm

	def connect(self):
		try:
			sdk_user = 'root'
			result = self._sdk_vm.login_in_guest(sdk_user, '', 0).wait()
			if not VMIO_PRESENT:
				self._sdk_vm.connect(0).wait()
			else:
				self._io = prlsdkapi.VmIO()
				self._io.connect_to_vm(self._sdk_vm).wait()
			self._sdk_vmguest = result.get_param()
		except:
			raise ConnectionError

	def disconnect(self):
		self._sdk_vmguest.logout().wait()
		if not VMIO_PRESENT:
			self._sdk_vm.disconnect()
		if hasattr(self,'_io'):
			self._io.disconnect_from_vm(self._sdk_vm)

	def run_program(self, cmd, **kw):
		if isinstance(cmd, basestring):
			args = [cmd]
		else:
			args = cmd
		args_list = prlsdkapi.StringList()
		for arg in args[1:]:
			args_list.add_item(arg)
		return self._sdk_vmguest.run_program(args[0], args_list,
					prlsdkapi.StringList(), **kw)


class VmListCommon(object):
	"""
	Parent class for managing list of currently active VE's
	"""
	active = {}
	done = []

	def __init__(self):
		self._helper = prlsdkapi.ApiHelper()
		self._helper.init(consts.PRL_VERSION_7X)
		self._server = prlsdkapi.Server()
		self._server.login_local().wait()
		self._server.get_uuid = lambda : '{host}'

	def _try_add_vm(self, vm):
		self.active[vm.get_uuid()] = VmGuestSessionCommon(vm)

	def refresh(self):
		vm_list = self._server.get_vm_list_ex(consts.PVTF_VM).wait()
		vm_state_jobs_list = [(vm, vm.get_state()) for vm in vm_list]
		running_vms = []
		for (vm, vm_state_job) in vm_state_jobs_list:
			if vm_state_job.wait().get_param().get_state() == consts.VMS_RUNNING:
				self._try_add_vm(vm)

	def for_each_active_VE(self, method):
		for uuid in self.active.keys():
			try:
				getattr(self.active[uuid], method)()
			except PrlSDKError:
				del self.active[uuid]
			except DoneException:
				del self.active[uuid]
				self.done.append(uuid)

	def __del__(self):
		self.active.clear()
		self._server.logoff().wait()
