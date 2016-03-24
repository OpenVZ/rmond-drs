#! /usr/bin/python -u
from stat_collector_common import *

class LinuxGuestSession(VmGuestSessionCommon):
	"""
	class for short term statistics collecting from linux guests
	"""
	def __init__ (self, sdk_vm):
		super(LinuxGuestSession, self).__init__(sdk_vm)
		self.connect()

	def start_write(self):
		with open(os.path.join(DIR, "drs-transport")) as lin_bin, open('/dev/null', 'w') as dev_null_w:
			self._cur_job = self.run_program(
				["sh","-c","cat > /bin/drs-transport; chmod +x /bin/drs-transport"],
				nFlags = consts.PFD_STDIN|consts.PFD_STDOUT|consts.PFD_STDERR,
				nStdin = lin_bin.fileno(),
				nStdout = sys.stdout.fileno(),
				nStderr = sys.stdout.fileno(),
				).wait()

class WindowsGuestSession(VmGuestSessionCommon):
	"""
	class for short term statistics collecting from windows guests
	"""
	def __init__ (self, sdk_vm):
		super(WindowsGuestSession, self).__init__(sdk_vm)
		self.connect()

#NB: for now this script works only for windows with python installed (i.e. from VZT deploy)
#In future it can be re-writed if in prlsdk/dispatcher support for guest-file-write from qemu-agent will be added.
	def start_write(self):
		with open(os.path.join(DIR, "drs-transport.exe")) as win_bin, open('/dev/null', 'w') as dev_null_w:
			self._cur_job = self.run_program(
				["python", "-u", "-c", r"with open(r'C:\Program Files\Qemu-ga\drs-transport.exe','wb') as f : f.write(__import__('sys').stdin.read())"],
				nFlags = consts.PFD_STDIN|consts.PFD_STDOUT|consts.PFD_STDERR,
				nStdin = win_bin.fileno(),
				nStdout = sys.stdout.fileno(),
				nStderr = sys.stdout.fileno(),
				).wait()

class VmList(VmListCommon):
	"""
	class for managing list of currently active VE's and collecting statistics
	from them. Full statistics per VE would be collected only once.
	"""
	def _try_add_vm(self, vm):
		uuid = vm.get_uuid()
		if uuid in self.active:
			return
		if uuid in self.done:
			return
		vm_type = vm.get_vm_type()
		os_type = vm.get_os_type()
		try:
			if os_type == consts.PVS_GUEST_TYPE_LINUX:
				self.active[uuid] = LinuxGuestSession(vm)
			elif os_type == consts.PVS_GUEST_TYPE_WINDOWS:
				self.active[uuid] = WindowsGuestSession(vm)
		except PrlSDKError:
			pass
		except ConnectionError:
			pass
		except DoneException:
			self.done.append(uuid)

	def write_drs_transport(self):
		#Try add all running VE's
		self.refresh()
		#Start writing to guest
		self.for_each_active_VE('start_write')
		#Wait all writes
		self.for_each_active_VE('disconnect')

if __name__ == '__main__':

	List = VmList()
	List.write_drs_transport()
