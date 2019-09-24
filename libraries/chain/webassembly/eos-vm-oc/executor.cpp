#include <eosio/chain/webassembly/eos-vm-oc/executor.hpp>
#include <eosio/chain/webassembly/eos-vm-oc/code_cache.hpp>
#include <eosio/chain/webassembly/eos-vm-oc/memory.hpp>
#include <eosio/chain/webassembly/eos-vm-oc/intrinsic_mapping.hpp>
#include <eosio/chain/webassembly/eos-vm-oc/intrinsic.hpp>
#include <eosio/chain/wasm_eosio_constraints.hpp>
#include <eosio/chain/apply_context.hpp>
#include <eosio/chain/transaction_context.hpp>
#include <eosio/chain/exceptions.hpp>
#include <eosio/chain/types.hpp>

#include <fc/scoped_exit.hpp>

#include <boost/hana/equal.hpp>

#include <mutex>

#include <asm/prctl.h>
#include <sys/prctl.h>
#include <sys/syscall.h>

extern "C" int arch_prctl(int code, unsigned long* addr);

namespace eosio { namespace chain { namespace eosvmoc {

static constexpr auto signal_sentinel = 0x4D56534F45534559ul;

static std::mutex inited_signal_mutex;
static bool inited_signal;

static void(*chained_handler)(int,siginfo_t*,void*);
[[noreturn]] static void segv_handler(int sig, siginfo_t* info, void* ctx)  {
   control_block* cb_in_main_segment;

   //a 0 GS value is an indicator an executor hasn't been active on this thread recently
   uint64_t current_gs;
   syscall(SYS_arch_prctl, ARCH_GET_GS, &current_gs);
   if(current_gs == 0)
      goto notus;

   cb_in_main_segment = reinterpret_cast<control_block*>(current_gs - memory::cb_offset);

   //as a double check that the control block pointer is what we expect, look for the magic
   if(cb_in_main_segment->magic != signal_sentinel)
      goto notus;

   //was wasm running? If not, this SEGV was not due to us
   if(cb_in_main_segment->is_running == false)
      goto notus;

   //was the segfault within code?
   if((uintptr_t)info->si_addr >= cb_in_main_segment->execution_thread_code_start &&
      (uintptr_t)info->si_addr < cb_in_main_segment->execution_thread_code_start+cb_in_main_segment->execution_thread_code_length)
         siglongjmp(*cb_in_main_segment->jmp, EOSVMOC_EXIT_CHECKTIME_FAIL);

   //was the segfault within data?
   if((uintptr_t)info->si_addr >= cb_in_main_segment->execution_thread_memory_start &&
      (uintptr_t)info->si_addr < cb_in_main_segment->execution_thread_memory_start+cb_in_main_segment->execution_thread_memory_length)
         siglongjmp(*cb_in_main_segment->jmp, EOSVMOC_EXIT_SEGV);

notus:
   if(chained_handler)
      chained_handler(sig, info, ctx);
   ::signal(sig, SIG_DFL);
   ::raise(sig);
   __builtin_unreachable();
}

static int32_t grow_memory(int32_t grow, int32_t max) {
   EOSVMOC_MEMORY_PTR_cb_ptr;
   uint64_t previous_page_count = cb_ptr->current_linear_memory_pages;
   int32_t grow_amount = grow;
   uint64_t max_pages = max;
   if(grow == 0)
      return (int32_t)cb_ptr->current_linear_memory_pages;
   if(previous_page_count + grow_amount > max_pages)
      return (int32_t)-1;

   uint64_t current_gs;
   arch_prctl(ARCH_GET_GS, &current_gs);
   current_gs += grow_amount * memory::stride;
   arch_prctl(ARCH_SET_GS, (unsigned long*)current_gs);
   cb_ptr->current_linear_memory_pages += grow_amount;

   if(grow_amount > 0)
      memset((void*)(cb_ptr->full_linear_memory_start + previous_page_count*64u*1024u), 0, grow_amount*64u*1024u);

   return (int32_t)previous_page_count;
}
static intrinsic grow_memory_intrinsic EOSVMOC_INTRINSIC_INIT_PRIORITY("eosvmoc_internal.grow_memory", IR::FunctionType::get(IR::ResultType::i32,{IR::ValueType::i32,IR::ValueType::i32}), (void*)&grow_memory,
  boost::hana::index_if(intrinsic_table, ::boost::hana::equal.to(BOOST_HANA_STRING("eosvmoc_internal.grow_memory"))).value()
);

//This is effectively overriding the eosio_exit intrinsic in wasm_interface
static void eosio_exit(int32_t code) {
   EOSVMOC_MEMORY_PTR_cb_ptr;
   siglongjmp(*cb_ptr->jmp, EOSVMOC_EXIT_CLEAN_EXIT);
   __builtin_unreachable();
}
static intrinsic eosio_exit_intrinsic("env.eosio_exit", IR::FunctionType::get(IR::ResultType::none,{IR::ValueType::i32}), (void*)&eosio_exit,
  boost::hana::index_if(intrinsic_table, ::boost::hana::equal.to(BOOST_HANA_STRING("env.eosio_exit"))).value()
);

static void throw_internal_exception(const std::string& s) {
   EOSVMOC_MEMORY_PTR_cb_ptr;
   *cb_ptr->eptr = std::make_exception_ptr(wasm_execution_error(FC_LOG_MESSAGE(error, s)));
   siglongjmp(*cb_ptr->jmp, EOSVMOC_EXIT_EXCEPTION);
   __builtin_unreachable();
}

#define DEFINE_EOSVMOC_TRAP_INTRINSIC(module,name) \
	void name(); \
	static intrinsic name##Function EOSVMOC_INTRINSIC_INIT_PRIORITY(#module "." #name,IR::FunctionType::get(),(void*)&name, \
     boost::hana::index_if(intrinsic_table, ::boost::hana::equal.to(BOOST_HANA_STRING(#module "." #name))).value() \
   ); \
	void name()

DEFINE_EOSVMOC_TRAP_INTRINSIC(eosvmoc_internal,depth_assert) {
   throw_internal_exception("Exceeded call depth maximum");
}

DEFINE_EOSVMOC_TRAP_INTRINSIC(eosvmoc_internal,div0_or_overflow) {
   throw_internal_exception("Division by 0 or integer overflow trapped");
}

DEFINE_EOSVMOC_TRAP_INTRINSIC(eosvmoc_internal,indirect_call_mismatch) {
   throw_internal_exception("Indirect call function type mismatch");
}

DEFINE_EOSVMOC_TRAP_INTRINSIC(eosvmoc_internal,indirect_call_oob) {
   throw_internal_exception("Indirect call index out of bounds");
}

DEFINE_EOSVMOC_TRAP_INTRINSIC(eosvmoc_internal,unreachable) {
   throw_internal_exception("Unreachable reached");
}

executor::executor(const code_cache_base& cc) {
   //XXX
   static bool once_is_enough;
   if(!once_is_enough) {
      printf("current_linear_memory_pages %li\n", -memory::cb_offset + offsetof(eosio::chain::eosvmoc::control_block, current_linear_memory_pages));
      printf("full_linear_memory_start %li\n", -memory::cb_offset + offsetof(eosio::chain::eosvmoc::control_block, full_linear_memory_start));
      printf("cb_offset %lu\n", memory::cb_offset);
      printf("remaining call depth: %li\n", -memory::cb_offset + offsetof(eosio::chain::eosvmoc::control_block, current_call_depth_remaining));
      printf("first intrinsic: %lu\n", memory::first_intrinsic_offset);
      printf("current code offset: %li\n", -memory::cb_offset + offsetof(eosio::chain::eosvmoc::control_block, running_code_base));
      once_is_enough = true;
   }

   //if we're the first executor created, go setup the signal handling. For now we'll just leave this attached forever
   if(std::lock_guard g(inited_signal_mutex); inited_signal == false) {
      struct sigaction sig_action, old_sig_action;
      sig_action.sa_sigaction = segv_handler;
      sigemptyset(&sig_action.sa_mask);
      sig_action.sa_flags = SA_SIGINFO | SA_NODEFER;
      sigaction(SIGSEGV, &sig_action, &old_sig_action);
      if(old_sig_action.sa_flags & SA_SIGINFO)
         chained_handler = old_sig_action.sa_sigaction;
      else if(old_sig_action.sa_handler != SIG_IGN && old_sig_action.sa_handler != SIG_DFL)
         chained_handler = (void (*)(int,siginfo_t*,void*))old_sig_action.sa_handler;

      inited_signal = true;
   }

   struct stat s;
   fstat(cc.fd(), &s);
   code_mapping = (uint8_t*)mmap(nullptr, s.st_size, PROT_EXEC|PROT_READ, MAP_SHARED, cc.fd(), 0);
   code_mapping_size = s.st_size;
   mapping_is_executable = true;
}

void executor::execute(const code_descriptor& code, const memory& mem, apply_context& context) {
   if(mapping_is_executable == false) {
      mprotect(code_mapping, code_mapping_size, PROT_EXEC|PROT_READ);
      mapping_is_executable = true;
   }

   //prepare initial memory, mutable globals, and table data
   if(code.starting_memory_pages > 0 ) {
      arch_prctl(ARCH_SET_GS, (unsigned long*)(mem.zero_page_memory_base()+code.starting_memory_pages*memory::stride));
      memset(mem.full_page_memory_base(), 0, 64u*1024u*code.starting_memory_pages);
   }
   else
      arch_prctl(ARCH_SET_GS, (unsigned long*)mem.zero_page_memory_base());
   memcpy(mem.full_page_memory_base() - code.initdata_prolouge_size, code_mapping + code.initdata_begin, code.initdata_size);

   control_block* const cb = mem.get_control_block();
   cb->magic = signal_sentinel;
   cb->execution_thread_code_start = (uintptr_t)code_mapping;
   cb->execution_thread_code_length = code_mapping_size;
   cb->execution_thread_memory_start = (uintptr_t)mem.start_of_memory_slices();
   cb->execution_thread_memory_length = mem.size_of_memory_slice_mapping();
   cb->ctx = &context;
   executors_exception_ptr = nullptr;
   cb->eptr = &executors_exception_ptr;
   cb->current_call_depth_remaining = eosio::chain::wasm_constraints::maximum_call_depth+2;
   cb->bouce_buffer_ptr = 0;
   cb->current_linear_memory_pages = code.starting_memory_pages;
   cb->full_linear_memory_start = (uintptr_t)mem.full_page_memory_base();
   cb->jmp = &executors_sigjmp_buf;
   cb->bounce_buffers = &executors_bounce_buffers;
   cb->running_code_base = (uintptr_t)(code_mapping + code.code_begin);
   cb->is_running = true;

   context.trx_context.transaction_timer.set_expiration_callback([](void* user) {
      executor* self = (executor*)user;
      syscall(SYS_mprotect, self->code_mapping, self->code_mapping_size, PROT_NONE);
      self->mapping_is_executable = false;
   }, this);
   context.trx_context.checktime(); //catch any expiration that might have occurred before setting up callback

   auto reset_is_running = fc::make_scoped_exit([cb](){cb->is_running = false;});
   auto reset_bounce_buffers = fc::make_scoped_exit([cb](){cb->bounce_buffers->clear();});
   auto reset_expiry_cb = fc::make_scoped_exit([&tt=context.trx_context.transaction_timer](){tt.set_expiration_callback(nullptr, nullptr);});

   void(*apply_func)(uint64_t, uint64_t, uint64_t) = (void(*)(uint64_t, uint64_t, uint64_t))(cb->running_code_base + code.apply_offset);

   switch(sigsetjmp(*cb->jmp, 0)) {
      case 0:
         code.start.visit(overloaded {
            [&](const no_offset&) {},
            [&](const intrinsic_ordinal& i) {
               void(*start_func)() = (void(*)())(*(uintptr_t*)((uintptr_t)mem.zero_page_memory_base() - memory::first_intrinsic_offset - i.ordinal*8));
               start_func();
            },
            [&](const code_offset& offs) {
               void(*start_func)() = (void(*)())(cb->running_code_base + offs.offset);
               start_func();
            }
         });
         apply_func(context.get_receiver().to_uint64_t(), context.get_action().account.to_uint64_t(), context.get_action().name.to_uint64_t());
         break;
      //case 1: clean eosio_exit
      case EOSVMOC_EXIT_CHECKTIME_FAIL:
         context.trx_context.checktime();
         break;
      case EOSVMOC_EXIT_SEGV:
         EOS_ASSERT(false, wasm_execution_error, "access violation");
         break;
      case EOSVMOC_EXIT_EXCEPTION: //exception
         std::rethrow_exception(*cb->eptr);
         break;
   }
}

executor::~executor() {
   arch_prctl(ARCH_SET_GS, nullptr);
}

}}}
