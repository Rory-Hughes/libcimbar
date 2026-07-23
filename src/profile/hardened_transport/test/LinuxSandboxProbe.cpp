/* This code is subject to the terms of the Mozilla Public License, v.2.0. http://mozilla.org/MPL/2.0/. */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "HardenedFountainTransport.h"

#include "fountain/FountainEncoder.h"
#include "fountain/FountainMetadata.h"

#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>

#include <fcntl.h>
#include <grp.h>
#include <linux/audit.h>
#include <linux/capability.h>
#include <linux/filter.h>
#include <linux/seccomp.h>
#include <sched.h>
#include <signal.h>
#include <sys/prctl.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#ifndef AUDIT_ARCH_X86_64
#error "Linux sandbox prototype currently expects AUDIT_ARCH_X86_64"
#endif

namespace {

constexpr unsigned chunk_size = 690U;
constexpr std::size_t object_size = 1200U;
constexpr std::size_t encoded_size = 10U * chunk_size;
constexpr unsigned sandbox_timeout_ms = 2000U;
using object_buffer = std::array<std::uint8_t, object_size>;
using encoded_buffer = std::array<char, encoded_size>;

struct SandboxChildResult
{
	std::int32_t setup_status;
	std::int32_t network_namespace_errno;
	std::int32_t effective_caps_nonzero;
	std::int32_t decode_exact;
	std::int32_t open_read_blocked;
	std::int32_t open_write_blocked;
	std::int32_t socket_blocked;
	std::int32_t fork_blocked;
	std::int32_t exec_blocked;
	std::int32_t setuid_blocked;
	std::int32_t no_new_privs_enabled;
	std::int32_t non_root_uid;
};

struct SandboxRunResult
{
	SandboxChildResult child{};
	pid_t pid = -1;
	int wait_status = 0;
	bool exited = false;
	bool timed_out = false;
};

FountainTransferPolicy profile_policy()
{
	FountainTransferPolicy policy;
	policy.object_class = FountainObjectClass::message;
	policy.decoder_limits.maximum_object_size = 2048U;
	policy.decoder_limits.maximum_active_object_bytes = 2048U;
	policy.decoder_limits.maximum_codec_memory_bytes = 1024U * 1024U;
	policy.decoder_limits.maximum_active_codec_memory_bytes = 1024U * 1024U;
	policy.decoder_limits.maximum_active_streams = 1U;
	policy.decoder_limits.maximum_completed_transfers = 0U;
	policy.decoder_limits.maximum_unique_blocks = 64U;
	policy.decoder_limits.maximum_block_id = 64U;
	policy.decoder_limits.maximum_packets_per_frame = 16U;
	policy.decoder_limits.maximum_frames_per_transfer = 64U;
	policy.decoder_limits.maximum_no_progress_frames = 16U;
	return policy;
}

bool encode(const object_buffer& input, std::uint8_t encode_id, encoded_buffer& encoded)
{
	constexpr unsigned metadata_size = FountainMetadata::md_size;
	constexpr unsigned payload_size = chunk_size - metadata_size;
	FountainEncoder encoder(
	    reinterpret_cast<const std::uint8_t*>(input.data()),
	    input.size(),
	    payload_size
	);
	if (!encoder.good())
		return false;

	unsigned block = 0U;
	for (std::size_t offset = 0U; offset < encoded.size(); offset += chunk_size)
	{
		auto* packet = reinterpret_cast<std::uint8_t*>(encoded.data() + offset);
		std::size_t written = encoder.encode(block++, packet + metadata_size, payload_size);
		if (written != payload_size)
			written = encoder.encode(block++, packet + metadata_size, payload_size);
		if (written != payload_size)
			return false;
		FountainMetadata::to_uint8_arr(
		    encode_id & 0x7FU,
		    static_cast<unsigned>(input.size()),
		    block - 1U,
		    packet
		);
	}
	return true;
}

bool decode_exact(const encoded_buffer& encoded, const object_buffer& expected)
{
	cimbar::HardenedFountainTransport transport(chunk_size, profile_policy());
	if (!transport.good())
		return false;
	if (transport.submit_frame(encoded.data(), static_cast<unsigned>(encoded.size())) != 1)
		return false;
	auto object = transport.take_completed_object();
	if (!object || object->size() != expected.size())
		return false;
	return std::equal(expected.begin(), expected.end(), object->begin());
}

bool set_limit(int resource, rlim_t soft, rlim_t hard)
{
	struct rlimit limit {};
	limit.rlim_cur = soft;
	limit.rlim_max = hard;
	return setrlimit(resource, &limit) == 0;
}

bool effective_capabilities_nonzero()
{
	__user_cap_header_struct header {};
	__user_cap_data_struct data[2] {};
	header.version = _LINUX_CAPABILITY_VERSION_3;
	header.pid = 0;
	if (syscall(SYS_capget, &header, data) != 0)
		return true;
	return data[0].effective != 0U || data[1].effective != 0U;
}

bool drop_root_identity()
{
	if (geteuid() != 0)
		return true;
	if (setgroups(0, nullptr) != 0 && errno != EPERM)
		return false;
	if (setresgid(65534, 65534, 65534) != 0)
		return false;
	if (setresuid(65534, 65534, 65534) != 0)
		return false;
	return geteuid() != 0;
}

#define ALLOW_SYSCALL(name) \
	BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_##name, 0, 1), \
	BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ALLOW)

bool install_seccomp_allowlist()
{
	struct sock_filter filter[] = {
	    BPF_STMT(BPF_LD | BPF_W | BPF_ABS, static_cast<unsigned>(offsetof(seccomp_data, arch))),
	    BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, AUDIT_ARCH_X86_64, 1, 0),
	    BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_KILL_PROCESS),
	    BPF_STMT(BPF_LD | BPF_W | BPF_ABS, static_cast<unsigned>(offsetof(seccomp_data, nr))),

	    ALLOW_SYSCALL(read),
	    ALLOW_SYSCALL(write),
	    ALLOW_SYSCALL(close),
	    ALLOW_SYSCALL(exit),
	    ALLOW_SYSCALL(exit_group),
	    ALLOW_SYSCALL(rt_sigreturn),
	    ALLOW_SYSCALL(rt_sigaction),
	    ALLOW_SYSCALL(rt_sigprocmask),
	    ALLOW_SYSCALL(brk),
	    ALLOW_SYSCALL(mmap),
	    ALLOW_SYSCALL(munmap),
	    ALLOW_SYSCALL(mremap),
	    ALLOW_SYSCALL(mprotect),
	    ALLOW_SYSCALL(madvise),
	    ALLOW_SYSCALL(clock_gettime),
	    ALLOW_SYSCALL(getpid),
	    ALLOW_SYSCALL(gettid),
	    ALLOW_SYSCALL(getuid),
	    ALLOW_SYSCALL(geteuid),
	    ALLOW_SYSCALL(getgid),
	    ALLOW_SYSCALL(getegid),
	    ALLOW_SYSCALL(futex),

	    BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ERRNO | EPERM),
	};
	struct sock_fprog program {};
	program.len = static_cast<unsigned short>(sizeof(filter) / sizeof(filter[0]));
	program.filter = filter;
	return prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &program) == 0;
}

#undef ALLOW_SYSCALL

std::int32_t apply_sandbox(
    std::int32_t& network_namespace_errno,
    std::int32_t& effective_caps_nonzero,
    std::int32_t& no_new_privs_enabled,
    std::int32_t& non_root_uid)
{
	if (!drop_root_identity())
		return 10;
	if (geteuid() == 0)
		return 11;
	if (!set_limit(RLIMIT_AS, 64U * 1024U * 1024U, 64U * 1024U * 1024U))
		return 12;
	if (!set_limit(RLIMIT_CPU, 1U, 1U))
		return 13;
	if (!set_limit(RLIMIT_NPROC, 1U, 1U))
		return 14;
	if (!set_limit(RLIMIT_NOFILE, 8U, 8U))
		return 15;
	if (!set_limit(RLIMIT_FSIZE, 0U, 0U))
		return 16;
	if (!set_limit(RLIMIT_CORE, 0U, 0U))
		return 17;
	if (prctl(PR_SET_DUMPABLE, 0) != 0)
		return 18;
	if (prctl(PR_SET_KEEPCAPS, 0) != 0)
		return 19;
	if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) != 0)
		return 20;

	network_namespace_errno = 0;
	if (unshare(CLONE_NEWNET) != 0)
		network_namespace_errno = errno;

	no_new_privs_enabled = prctl(PR_GET_NO_NEW_PRIVS, 0, 0, 0, 0) == 1;
	non_root_uid = geteuid() != 0;
	effective_caps_nonzero = effective_capabilities_nonzero();

	if (!install_seccomp_allowlist())
		return 21;
	return 0;
}

bool syscall_blocked(long result)
{
	return result == -1 && errno == EPERM;
}

SandboxChildResult run_sandboxed_decoder(
    const encoded_buffer& encoded,
    const object_buffer& expected,
    const char* secret_path)
{
	SandboxChildResult result {};
	result.setup_status = apply_sandbox(
	    result.network_namespace_errno,
	    result.effective_caps_nonzero,
	    result.no_new_privs_enabled,
	    result.non_root_uid
	);
	if (result.setup_status != 0)
		return result;

	result.decode_exact = decode_exact(encoded, expected);

	errno = 0;
	const long read_fd = syscall(SYS_openat, AT_FDCWD, secret_path, O_RDONLY | O_CLOEXEC, 0);
	result.open_read_blocked = syscall_blocked(read_fd);
	if (read_fd >= 0)
		close(static_cast<int>(read_fd));

	errno = 0;
	const long write_fd = syscall(
	    SYS_openat,
	    AT_FDCWD,
	    secret_path,
	    O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC,
	    0600
	);
	result.open_write_blocked = syscall_blocked(write_fd);
	if (write_fd >= 0)
		close(static_cast<int>(write_fd));

	errno = 0;
	const long socket_fd = syscall(SYS_socket, AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
	result.socket_blocked = syscall_blocked(socket_fd);
	if (socket_fd >= 0)
		close(static_cast<int>(socket_fd));

	errno = 0;
	const long fork_result = syscall(SYS_fork);
	if (fork_result == 0)
		_exit(99);
	result.fork_blocked = syscall_blocked(fork_result);

	const char* argv[] = {"/bin/true", nullptr};
	char* const* exec_argv = const_cast<char* const*>(argv);
	char* const exec_env[] = {nullptr};
	errno = 0;
	const long exec_result = syscall(SYS_execve, "/bin/true", exec_argv, exec_env);
	result.exec_blocked = syscall_blocked(exec_result);

	errno = 0;
	const long setuid_result = syscall(SYS_setuid, 0);
	result.setuid_blocked = syscall_blocked(setuid_result);
	return result;
}

bool write_exact(int fd, const void* data, std::size_t size)
{
	const auto* cursor = static_cast<const std::uint8_t*>(data);
	while (size > 0U)
	{
		const ssize_t written = write(fd, cursor, size);
		if (written <= 0)
			return false;
		cursor += static_cast<std::size_t>(written);
		size -= static_cast<std::size_t>(written);
	}
	return true;
}

bool read_exact(int fd, void* data, std::size_t size)
{
	auto* cursor = static_cast<std::uint8_t*>(data);
	while (size > 0U)
	{
		const ssize_t bytes = read(fd, cursor, size);
		if (bytes <= 0)
			return false;
		cursor += static_cast<std::size_t>(bytes);
		size -= static_cast<std::size_t>(bytes);
	}
	return true;
}

SandboxRunResult run_transfer_in_child(
    const encoded_buffer& encoded,
    const object_buffer& expected,
    const char* secret_path)
{
	SandboxRunResult result;
	int pipe_fds[2] = {-1, -1};
	if (pipe2(pipe_fds, O_CLOEXEC) != 0)
		return result;

	const pid_t child = fork();
	if (child < 0)
	{
		close(pipe_fds[0]);
		close(pipe_fds[1]);
		return result;
	}
	if (child == 0)
	{
		close(pipe_fds[0]);
		const SandboxChildResult child_result =
		    run_sandboxed_decoder(encoded, expected, secret_path);
		const bool wrote = write_exact(pipe_fds[1], &child_result, sizeof(child_result));
		close(pipe_fds[1]);
		_exit(wrote ? 0 : 98);
	}

	result.pid = child;
	close(pipe_fds[1]);
	for (unsigned elapsed = 0U; elapsed < sandbox_timeout_ms; elapsed += 10U)
	{
		const pid_t wait_result = waitpid(child, &result.wait_status, WNOHANG);
		if (wait_result == child)
		{
			result.exited = true;
			break;
		}
		if (wait_result < 0)
			break;
		usleep(10U * 1000U);
	}
	if (!result.exited)
	{
		result.timed_out = true;
		kill(child, SIGKILL);
		waitpid(child, &result.wait_status, 0);
	}

	const bool read_result = read_exact(pipe_fds[0], &result.child, sizeof(result.child));
	close(pipe_fds[0]);
	if (!read_result)
		result.child.setup_status = 90;
	return result;
}

bool child_result_ok(const SandboxChildResult& result)
{
	if (result.setup_status != 0)
		return false;
	if (!result.decode_exact ||
	    !result.open_read_blocked ||
	    !result.open_write_blocked ||
	    !result.socket_blocked ||
	    !result.fork_blocked ||
	    !result.exec_blocked ||
	    !result.setuid_blocked ||
	    !result.no_new_privs_enabled ||
	    !result.non_root_uid)
	{
		return false;
	}
	return result.effective_caps_nonzero == 0;
}

bool run_result_ok(const SandboxRunResult& result)
{
	return result.pid > 0 &&
	       result.exited &&
	       !result.timed_out &&
	       WIFEXITED(result.wait_status) &&
	       WEXITSTATUS(result.wait_status) == 0 &&
	       child_result_ok(result.child);
}

bool make_secret_file(char* directory_template, char* secret_path, std::size_t secret_path_size)
{
	if (mkdtemp(directory_template) == nullptr)
		return false;
	const int written = std::snprintf(
	    secret_path,
	    secret_path_size,
	    "%s/secret.txt",
	    directory_template
	);
	if (written <= 0 || static_cast<std::size_t>(written) >= secret_path_size)
		return false;
	const int fd = open(secret_path, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0600);
	if (fd < 0)
		return false;
	const char secret[] = "secure-core-secret";
	const bool ok = write_exact(fd, secret, sizeof(secret) - 1U);
	close(fd);
	return ok;
}

void print_result(const char* label, const SandboxRunResult& result)
{
	std::fprintf(
	    stderr,
	    "%s: pid=%ld exited=%d timed_out=%d wait_status=%d setup=%d netns_errno=%d "
	    "caps_nonzero=%d decode=%d open_read=%d open_write=%d socket=%d fork=%d "
	    "exec=%d setuid=%d no_new_privs=%d non_root=%d\n",
	    label,
	    static_cast<long>(result.pid),
	    result.exited ? 1 : 0,
	    result.timed_out ? 1 : 0,
	    result.wait_status,
	    result.child.setup_status,
	    result.child.network_namespace_errno,
	    result.child.effective_caps_nonzero,
	    result.child.decode_exact,
	    result.child.open_read_blocked,
	    result.child.open_write_blocked,
	    result.child.socket_blocked,
	    result.child.fork_blocked,
	    result.child.exec_blocked,
	    result.child.setuid_blocked,
	    result.child.no_new_privs_enabled,
	    result.child.non_root_uid
	);
}

} // namespace

int main()
{
	object_buffer expected {};
	for (std::size_t index = 0U; index < expected.size(); ++index)
		expected[index] = static_cast<std::uint8_t>((index * 17U + 91U) & 0xFFU);

	encoded_buffer encoded {};
	if (!encode(expected, 42U, encoded))
		return 1;

	char directory_template[] = "/tmp/libcimbar-sandbox-XXXXXX";
	char secret_path[256] {};
	if (!make_secret_file(directory_template, secret_path, sizeof(secret_path)))
		return 2;

	const SandboxRunResult first = run_transfer_in_child(encoded, expected, secret_path);
	const SandboxRunResult second = run_transfer_in_child(encoded, expected, secret_path);

	unlink(secret_path);
	rmdir(directory_template);

	if (!run_result_ok(first))
	{
		print_result("first", first);
		return 10 + first.child.setup_status;
	}
	if (!run_result_ok(second))
	{
		print_result("second", second);
		return 30 + second.child.setup_status;
	}
	if (first.pid == second.pid)
	{
		print_result("first", first);
		print_result("second", second);
		return 70;
	}
	return 0;
}
