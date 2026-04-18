#include "renamer.h"

renamer::renamer(uint64_t logic_reg_count,
                 uint64_t phys_reg_count,
                 uint64_t branch_count,
                 uint64_t active_entry_count)
{
	num_logic_regs = logic_reg_count;
	num_phys_regs = phys_reg_count;
	num_branches = branch_count;
	num_active = active_entry_count;

	assert(num_phys_regs > num_logic_regs);
	 assert(num_branches >=1 && num_branches <=64);
	assert(num_active > 0);

	Rename_Map_Table.resize(num_logic_regs);
	Architectural_Map_Table.resize(num_logic_regs);

	Physical_Register_File.resize(num_phys_regs, 0);
	Ready_Bits.resize(num_phys_regs, 0);

	for (uint64_t l = 0; l < num_logic_regs; l++) {
		Architectural_Map_Table[l] = l;
		Rename_Map_Table[l] = l;
		Ready_Bits[l] = 1;
	}

	if (num_branches ==64) 
		valid_branch_mask = ~0ULL;
	else 
		valid_branch_mask = (1ULL << num_branches) - 1ULL;

	Global_Branch_Mask = 0;

	Branch_Checkpoints.resize(num_branches);
	for (uint64_t i = 0; i < num_branches; i++) {
		Branch_Checkpoints[i].shadow_map_table.resize(num_logic_regs);
		for (uint64_t l = 0; l < num_logic_regs; l++) {
			Branch_Checkpoints[i].shadow_map_table[l] = 0;
		}
		Branch_Checkpoints[i].free_list_head = 0;
		Branch_Checkpoints[i].free_list_head_phase = 0;
		Branch_Checkpoints[i].checkpointed_gbm = 0;
	}

	Free_List_size = num_phys_regs - num_logic_regs;
	Free_List.resize(Free_List_size);

	for (uint64_t i = 0; i < Free_List_size; i++) {
		Free_List[i] = num_logic_regs + i;
	}

	Free_List_head = 0;
	 Free_List_tail = 0;
	Free_List_head_phase = 0;
	Free_List_tail_phase = 1; // phases differ = FULL

	// Active List empty
	Active_List_size = num_active;
	Active_List.resize(Active_List_size);
	for (uint64_t i = 0; i < Active_List_size; i++) {
		Active_List[i].dest_valid = false;
		 Active_List[i].log_reg    = 0;
		Active_List[i].phys_reg   = 0;

		Active_List[i].completed  = false;

		Active_List[i].exception  = false;
		 Active_List[i].load_viol  = false;
		 Active_List[i].br_misp    = false;
		Active_List[i].val_misp   = false;

		Active_List[i].load       = false;
		 Active_List[i].store      = false;
		Active_List[i].branch     = false;
		 Active_List[i].amo        = false;
		Active_List[i].csr        = false;

		Active_List[i].PC         = 0;
	}

	Active_List_head = 0;
	Active_List_tail = 0;
	 Active_List_head_phase = 0;
	Active_List_tail_phase = 0;
}

renamer::~renamer() {
}

bool renamer::stall_reg( uint64_t bundle_dst) {
	return (free_list_count() < bundle_dst);
}

bool renamer::stall_branch(uint64_t bundle_branch ) {
	uint64_t inuse = Global_Branch_Mask & valid_branch_mask;
	uint64_t used = __builtin_popcountll(inuse);
	uint64_t free = num_branches - used;
	return (free < bundle_branch);
}

uint64_t renamer::get_branch_mask( ) {
	return (Global_Branch_Mask & valid_branch_mask);
}

uint64_t renamer::rename_rsrc( uint64_t log_reg) {
	assert(log_reg < num_logic_regs);
	return Rename_Map_Table[log_reg];
}

uint64_t renamer::rename_rdst(uint64_t log_reg ) {
	assert(log_reg < num_logic_regs);
	 assert(!free_list_empty());

	uint64_t p = free_list_pop();
	Rename_Map_Table[log_reg] = p;
	//We need to make newly alllocated destination regs not Ready_Bits
	Ready_Bits[p] = 0;

	return p;
}

uint64_t renamer::checkpoint( ) {
	uint64_t inuse = Global_Branch_Mask & valid_branch_mask;

	uint64_t id = 0;
	bool found = false;
	for (uint64_t i = 0; i < num_branches; i++) {
		if (((inuse >> i) & 1ULL) == 0ULL) {
			id = i;
			found = true;
			break;
		}
	}
	assert(found);

	// Store PREVIOUS Global_Branch_Mask
	Branch_Checkpoints[id].checkpointed_gbm = Global_Branch_Mask;

	for (uint64_t l = 0; l < num_logic_regs; l++) {
		Branch_Checkpoints[id].shadow_map_table[l] = Rename_Map_Table[l];
	}

	// Checkpoint Free_List head(+phase)
	Branch_Checkpoints[id].free_list_head = Free_List_head;
	Branch_Checkpoints[id].free_list_head_phase = Free_List_head_phase;

	// Allocate Global_Branch_Mask bit
	Global_Branch_Mask |= (1ULL << id);
	Global_Branch_Mask &= valid_branch_mask;

	return id;
}

bool renamer::stall_dispatch( uint64_t bundle_inst) {
	return (active_list_free() < bundle_inst);
}

uint64_t renamer::dispatch_inst(bool dest_valid,
                                uint64_t log_reg,
                                uint64_t phys_reg,
                                bool load,
                                bool store,
                                bool branch,
                                bool amo,
                                bool csr,
                                uint64_t PC)
{
	assert(!active_list_full());

	uint64_t idx = Active_List_tail;

	Active_List[idx].dest_valid = dest_valid;
	 Active_List[idx].log_reg = log_reg;
	  Active_List[idx].phys_reg = phys_reg;

	Active_List[idx].completed = false;

	Active_List[idx].exception = false;
	 Active_List[idx].load_viol = false;
	Active_List[idx].br_misp = false;
	 Active_List[idx].val_misp = false;

	Active_List[idx].load = load;
	 Active_List[idx].store = store;
	 Active_List[idx].branch = branch;
	 Active_List[idx].amo = amo;
	Active_List[idx].csr = csr;

	Active_List[idx].PC = PC;

	advance_ptr(Active_List_tail, Active_List_tail_phase, Active_List_size);

	return idx;
}

bool renamer::is_ready(uint64_t phys_reg ) {
	assert(phys_reg < num_phys_regs);
	return (Ready_Bits[phys_reg] != 0);
}

void renamer::clear_ready( uint64_t phys_reg) {
	 assert(phys_reg < num_phys_regs);
	Ready_Bits[phys_reg] = 0;   
}

uint64_t renamer::read( uint64_t phys_reg) {
	assert(phys_reg < num_phys_regs);
	return Physical_Register_File[phys_reg];
}

void renamer::set_ready(uint64_t phys_reg ) {
	assert(phys_reg < num_phys_regs);
	Ready_Bits[phys_reg] = 1;
}

void renamer::write( uint64_t phys_reg, uint64_t value) {
	assert(phys_reg < num_phys_regs);  
	Physical_Register_File[phys_reg] = value;  
}

void renamer::set_complete(uint64_t AL_index) {
	assert(AL_index < Active_List_size);
	 Active_List[AL_index].completed = true;
}

void renamer::resolve(uint64_t AL_index, uint64_t branch_ID, bool correct) {
	assert(branch_ID < num_branches);
	 assert(AL_index < Active_List_size);

	uint64_t bit = (1ULL << branch_ID);

	if (correct) {
		Global_Branch_Mask &= ~bit;
     
		// Clr bit in all checkpointed GBMs
		for (uint64_t i = 0; i < num_branches; i++) {
			Branch_Checkpoints[i].checkpointed_gbm &= ~bit;
		}

		Global_Branch_Mask &= valid_branch_mask;
		return;
	}

	// Restore Global_Branch_Mask from checkpoint
	Global_Branch_Mask = Branch_Checkpoints[branch_ID].checkpointed_gbm;  

	// Clear mispredicted branch bit
	Global_Branch_Mask &= ~bit;
   
	Global_Branch_Mask &= valid_branch_mask;

	// Restore Rename_Map_Table from checkpoint SMT
	for (uint64_t l = 0; l < num_logic_regs; l++) {
		Rename_Map_Table[l] = Branch_Checkpoints[branch_ID].shadow_map_table[l];
	}

	// Restore Free_List head(+phase)
	Free_List_head = Branch_Checkpoints[branch_ID].free_list_head;
	Free_List_head_phase = Branch_Checkpoints[branch_ID].free_list_head_phase;
 
	// Restore Active_List tail to entry after the branch
	uint64_t new_tail_idx = (AL_index + 1) % Active_List_size;

	// get branch phase
	uint8_t branch_phase = (AL_index >= Active_List_head) ? Active_List_head_phase : (Active_List_head_phase ^ 1);

	// flip tail phase if only it wraps around
	 uint8_t new_tail_phase = (AL_index == (Active_List_size - 1)) ? (branch_phase ^ 1) : branch_phase;

	Active_List_tail = new_tail_idx;
	Active_List_tail_phase = new_tail_phase;

	assert((Global_Branch_Mask & bit) == 0ULL);
	 assert(!active_list_empty());
	assert(!(Active_List_tail == Active_List_head && Active_List_tail_phase == Active_List_head_phase));
}

bool renamer::precommit(bool &completed,
                        bool &exception, bool &load_viol, bool &br_misp, bool &val_misp,
                        bool &load, bool &store, bool &branch, bool &amo, bool &csr,
                        uint64_t &PC)
{
	if (active_list_empty()) {
		return false;
	}

	const Active_List_Entry &e = Active_List[Active_List_head];

	completed = e.completed;
	exception = e.exception;
   load_viol = e.load_viol;
	br_misp = e.br_misp;
	 val_misp = e.val_misp;

	load = e.load;
	store = e.store;
	 branch = e.branch;
	amo = e.amo;
	 csr = e.csr;

	PC = e.PC;

	return true;
}

void renamer::commit() {
	assert(!active_list_empty());

	Active_List_Entry &e = Active_List[Active_List_head];

	 assert(e.completed);
	assert(!e.exception);
	assert(!e.load_viol);

	if (e.dest_valid) {
		assert(e.log_reg < num_logic_regs);
		assert(e.phys_reg < num_phys_regs);

		uint64_t l = e.log_reg;
	     uint64_t p_new = e.phys_reg;
		uint64_t p_old = Architectural_Map_Table[l];

		Architectural_Map_Table[l] = p_new;

		free_list_push(p_old);
	}

	advance_ptr(Active_List_head, Active_List_head_phase, Active_List_size);
}

void renamer::squash() {
	Global_Branch_Mask = 0;

	// Rename_Map_Table = Architectural_Map_Table
	for (uint64_t l = 0; l < num_logic_regs; l++) {
		Rename_Map_Table[l] = Architectural_Map_Table[l];
	}

	// Empty Active_List
	Active_List_head = 0;
	Active_List_tail = 0;
	 Active_List_head_phase = 0;
	Active_List_tail_phase = 0;

	vector<uint8_t> used(num_phys_regs, 0);
	for (uint64_t l = 0; l < num_logic_regs; l++) {
		used[Architectural_Map_Table[l]] = 1;
	}

	uint64_t idx = 0;
	for (uint64_t p = 0; p < num_phys_regs; p++) {
		if (!used[p]) {
			assert(idx < Free_List_size);
			Free_List[idx++] = p;
		}
	}
	assert(idx == Free_List_size);

	Free_List_head = 0;
	 Free_List_tail = 0;
	Free_List_head_phase = 0;
	Free_List_tail_phase = 1; // full

	for (uint64_t p = 0; p < num_phys_regs; p++) Ready_Bits[p] = 0;
	for (uint64_t l = 0; l < num_logic_regs; l++) Ready_Bits[Architectural_Map_Table[l]] = 1;
}

void renamer::set_exception(uint64_t AL_index) {
	  assert(AL_index < Active_List_size);
	Active_List[AL_index].exception = true;
}

void renamer::set_load_violation(uint64_t AL_index) {
	assert(AL_index < Active_List_size);
	Active_List[AL_index].load_viol = true;
}

void renamer::set_branch_misprediction(uint64_t AL_index) {
	assert(AL_index < Active_List_size);
	 Active_List[AL_index].br_misp = true;
}

void renamer::set_value_misprediction(uint64_t AL_index) {
	  assert(AL_index < Active_List_size);
	Active_List[AL_index].val_misp = true;
}

bool renamer::get_exception(uint64_t AL_index) {
	 assert(AL_index < Active_List_size);

	 return Active_List[AL_index].exception;
}
