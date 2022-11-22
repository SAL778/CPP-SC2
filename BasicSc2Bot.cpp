#include "BasicSc2Bot.h"
#include "Directive.h"
#include "Mob.h"
#include "LocationHandler.h"
#include "sc2api/sc2_api.h"
#include "sc2api/sc2_args.h"
#include "sc2api/sc2_client.h"
#include "sc2lib/sc2_lib.h"
#include "sc2utils/sc2_manage_process.h"
#include "sc2utils/sc2_arg_parser.h"

// forward declarations
class TriggerCondition;
class Mob;
class MobHandler;


void BasicSc2Bot::setCurrentStrategy(Strategy* strategy_) {
	// set the current strategy
	storeStrategy(*strategy_);
}

void BasicSc2Bot::addStrat(Precept strategy) {
	strategies.push_back(strategy);
}

bool BasicSc2Bot::AssignNearbyWorkerToGasStructure(const sc2::Unit& gas_structure) {
	// get a nearby worker unit that is not currently assigned to gas
	// and assign it to harvest gas

	bool found_viable_unit = false;
	
	// get a filtered set of workers that are currently assigned to minerals
	std::unordered_set<FLAGS> flags{ FLAGS::IS_WORKER, FLAGS::IS_MINERAL_GATHERER };
	std::unordered_set<Mob*> worker_miners = mobH->filter_by_flags(mobH->get_mobs(), flags);

	//std::unordered_set<Mob*> worker_miners = filter_by_flag(mobs, FLAGS::IS_MINERAL_GATHERER);
	float distance = std::numeric_limits<float>::max();
	Mob* target = nullptr;
	for (Mob* m : worker_miners) {
		float d = sc2::DistanceSquared2D(m->unit.pos, gas_structure.pos);
		if (d < distance) {
			distance = d;
			target = m;
			found_viable_unit = true;
		}
	}
	if (found_viable_unit) {
		target->remove_flag(FLAGS::IS_MINERAL_GATHERER);
		target->set_flag(FLAGS::IS_GAS_GATHERER);

		// make the unit continue to mine gas after being idle
		Directive directive_get_gas(Directive::DEFAULT_DIRECTIVE, Directive::GET_GAS_NEAR_LOCATION, target->unit.unit_type, sc2::ABILITY_ID::HARVEST_GATHER, gas_structure.pos);
		target->assignDefaultDirective(directive_get_gas);
		Actions()->UnitCommand(&(target->unit), sc2::ABILITY_ID::HARVEST_GATHER, &gas_structure);


		return true;
	}
	return false;
}

void BasicSc2Bot::storeDirective(Directive directive_)
{
	if (directive_by_id[directive_.getID()])
		return;

	directive_storage.emplace_back(std::make_unique<Directive>(directive_));
	Directive* created_dir = directive_storage.back().get();
	stored_directives.insert(created_dir);
	directive_by_id[directive_.getID()] = created_dir;
}

void BasicSc2Bot::storeStrategy(Strategy strategy_)
{
	// only one strategy can be stored
	if (strategy_storage.empty()) {
		strategy_storage.emplace_back(std::make_unique<Strategy>(strategy_));
		current_strategy = strategy_storage.back().get();
	}
}

Directive* BasicSc2Bot::getLastStoredDirective()
{
	return directive_storage.back().get();
}


bool BasicSc2Bot::have_upgrade(const sc2::UpgradeID upgrade_) {
	// return true if the bot has fully researched the specified upgrade

	const sc2::ObservationInterface* observation = Observation();
	const std::vector<sc2::UpgradeID> upgrades = observation->GetUpgrades();
	return (std::find(upgrades.begin(), upgrades.end(), upgrade_) != upgrades.end());
}


bool BasicSc2Bot::can_unit_use_ability(const sc2::Unit& unit, const sc2::ABILITY_ID ability_) {
	// check if a unit is able to use a given ability

	sc2::QueryInterface* query_interface = Query();
	std::vector<sc2::AvailableAbility> abilities = (query_interface->GetAbilitiesForUnit(&unit)).abilities;
	for (auto a : abilities) {
		if (a.ability_id == ability_) {
			return true;
		}
	}
	return false;
}



bool BasicSc2Bot::is_structure(const sc2::Unit* unit) {
	// check if unit is a structure

	std::vector<sc2::Attribute> attrs = get_attributes(unit);
	return (std::find(attrs.begin(), attrs.end(), sc2::Attribute::Structure) != attrs.end());
}

void BasicSc2Bot::storeUnitType(std::string identifier_, sc2::UNIT_TYPEID unit_type_)
{
	// store a special unit, used for certain functions
	// e.g. "_CHRONOBOOST_TARGET" : the unit type of a building that chronoboost will exclusively target
	special_units[identifier_] = unit_type_;
}

void BasicSc2Bot::storeLocation(std::string identifier_, sc2::Point2D location_) {
	// store a special location
	// can be used in strategies to assign specific locations to later reference
	special_locations[identifier_] = location_;
}

bool BasicSc2Bot::is_mineral_patch(const sc2::Unit* unit_) {
	// check whether a given unit is a mineral patch

	sc2::UNIT_TYPEID type_ = unit_->unit_type;
	return (type_ == sc2::UNIT_TYPEID::NEUTRAL_BATTLESTATIONMINERALFIELD750 ||
		type_ == sc2::UNIT_TYPEID::NEUTRAL_BATTLESTATIONMINERALFIELD ||
		type_ == sc2::UNIT_TYPEID::NEUTRAL_LABMINERALFIELD750 ||
		type_ == sc2::UNIT_TYPEID::NEUTRAL_LABMINERALFIELD ||
		type_ == sc2::UNIT_TYPEID::NEUTRAL_MINERALFIELD750 ||
		type_ == sc2::UNIT_TYPEID::NEUTRAL_MINERALFIELD ||
		type_ == sc2::UNIT_TYPEID::NEUTRAL_PURIFIERMINERALFIELD750 ||
		type_ == sc2::UNIT_TYPEID::NEUTRAL_PURIFIERMINERALFIELD ||
		type_ == sc2::UNIT_TYPEID::NEUTRAL_PURIFIERRICHMINERALFIELD750 ||
		type_ == sc2::UNIT_TYPEID::NEUTRAL_PURIFIERRICHMINERALFIELD ||
		type_ == sc2::UNIT_TYPEID::NEUTRAL_RICHMINERALFIELD750 ||
		type_ == sc2::UNIT_TYPEID::NEUTRAL_RICHMINERALFIELD);
}

sc2::UNIT_TYPEID BasicSc2Bot::getUnitType(std::string identifier_)
{
	return special_units[identifier_];
	
}

sc2::Point2D BasicSc2Bot::getStoredLocation(std::string identifier_)
{
	return special_locations[identifier_];
}

int BasicSc2Bot::getMapIndex()
{
	// 1: cactus
	// 2: belshir
	// 3: proxima
	return map_index;
}

bool BasicSc2Bot::is_geyser(const sc2::Unit* unit_) {
	// check whether a given unit is a geyser

	sc2::UNIT_TYPEID type_ = unit_->unit_type;
	return (type_ == sc2::UNIT_TYPEID::NEUTRAL_VESPENEGEYSER ||
		type_ == sc2::UNIT_TYPEID::NEUTRAL_PROTOSSVESPENEGEYSER ||
		type_ == sc2::UNIT_TYPEID::NEUTRAL_SPACEPLATFORMGEYSER ||
		type_ == sc2::UNIT_TYPEID::NEUTRAL_PURIFIERVESPENEGEYSER ||
		type_ == sc2::UNIT_TYPEID::NEUTRAL_SHAKURASVESPENEGEYSER ||
		type_ == sc2::UNIT_TYPEID::NEUTRAL_RICHVESPENEGEYSER);
}


std::vector<sc2::Attribute> BasicSc2Bot::get_attributes(const sc2::Unit* unit) {
	// get attributes for a unit

	return getUnitTypeData(unit).attributes;
}

sc2::UnitTypeData BasicSc2Bot::getUnitTypeData(const sc2::Unit* unit) {
	// get UnitTypeData for a unit

	return Observation()->GetUnitTypeData()[unit->unit_type];
}

void BasicSc2Bot::initVariables() {
	const sc2::ObservationInterface* observation = Observation();
	map_name = observation->GetGameInfo().map_name;
	std::cout << "map_name: " << map_name;
	if (map_name == "Proxima Station LE" || map_name == "Proxim Station LE (Void)")
		map_index = 3; else
		if (map_name == "Bel'Shir Vestige LE" || map_name == "Bel'Shir Vestige LE (Void)")
			map_index = 2; else
			if (map_name == "Cactus Valley LE" || map_name == "Cactus Valley LE (Void)")
				map_index = 1; else
				map_index = 0;

	player_start_id = locH->getPlayerIDForMap(map_index, observation->GetStartLocation());
	locH->initLocations(map_index, player_start_id);

	std::cout << "Map Index " << map_index << std::endl;
	std::cout << "Map Name: " << map_name << std::endl;
	std::cout << "Player Start ID: " << player_start_id << std::endl;
}

void BasicSc2Bot::initStartingUnits() {
	// add all starting units to their respective mobs
	const sc2::ObservationInterface* observation = Observation();
	sc2::Units units = observation->GetUnits(sc2::Unit::Alliance::Self);
	for (const sc2::Unit* u : units) {
		sc2::UNIT_TYPEID u_type = u->unit_type;
		if (u_type == sc2::UNIT_TYPEID::TERRAN_SCV ||
			u_type == sc2::UNIT_TYPEID::ZERG_DRONE ||
			u_type == sc2::UNIT_TYPEID::PROTOSS_PROBE) 
		{
			Mob worker (*u, MOB::MOB_WORKER);
			Directive directive_get_minerals_near_Base(Directive::DEFAULT_DIRECTIVE, Directive::GET_MINERALS_NEAR_LOCATION, u_type, sc2::ABILITY_ID::HARVEST_GATHER, locH->getStartLocation());
			storeDirective(directive_get_minerals_near_Base);
			Directive* dir = getLastStoredDirective();
			worker.assignDefaultDirective(*dir);
			mobH->addMob(worker);
		}
		if (u_type == sc2::UNIT_TYPEID::PROTOSS_NEXUS ||
			u_type == sc2::UNIT_TYPEID::TERRAN_COMMANDCENTER ||
			u_type == sc2::UNIT_TYPEID::TERRAN_COMMANDCENTERFLYING ||
			u_type == sc2::UNIT_TYPEID::TERRAN_ORBITALCOMMAND ||
			u_type == sc2::UNIT_TYPEID::TERRAN_ORBITALCOMMANDFLYING ||
			u_type == sc2::UNIT_TYPEID::TERRAN_PLANETARYFORTRESS ||
			u_type == sc2::UNIT_TYPEID::ZERG_HATCHERY ||
			u_type == sc2::UNIT_TYPEID::ZERG_HIVE ||
			u_type == sc2::UNIT_TYPEID::ZERG_LAIR) {
			Mob townhall(*u, MOB::MOB_TOWNHALL);
			mobH->addMob(townhall);
		}
	}
}

void BasicSc2Bot::OnGameStart() {
	Strategy* strategy = new Strategy(this);
	setCurrentStrategy(strategy);

	mobH = new MobHandler(this); // initialize mob handler 
	locH = new LocationHandler(this);
	BasicSc2Bot::initVariables(); // initialize this first
	BasicSc2Bot::initStartingUnits();
	sc2::Point2D start_location = locH->getStartLocation();
	sc2::Point2D proxy_location = locH->getProxyLocation();
	std::cout << "Start Location: " << start_location.x << "," << start_location.y << std::endl;
	std::cout << "Build Area 0: " << locH->bases[0].get_build_area(0).x << "," << locH->bases[0].get_build_area(0).y << std::endl;
	std::cout << "Proxy Location: " << proxy_location.x << "," << proxy_location.y << std::endl;

	current_strategy->loadStrategies();
	//std::cout << "the pointer at game start: " << current_strategy << std::endl;
}

void::BasicSc2Bot::OnStep_100() {
	// occurs every 100 steps
}

void::BasicSc2Bot::OnStep_1000() {
	// occurs every 1000 steps
	std::cout << ".";
}

void BasicSc2Bot::OnStep() {
	const sc2::ObservationInterface* observation = Observation();
	int gameloop = observation->GetGameLoop();
	if (gameloop % 100 == 0) {
		OnStep_100();
	}
	if (gameloop % 1000 == 0) {
		OnStep_1000();
	}

	// update visibility data for chunks
	locH->scanChunks(observation);

	/*
	std::unordered_set<Mob*> busy_mobs = mobH->get_busy_mobs();
	if (!busy_mobs.empty()) {
		for (auto it = busy_mobs.begin(); it != busy_mobs.end(); ++it) {
			Mob* m = *it;
			Directive* dir = m->getCurrentDirective();

			if (dir) {
				// if a busy mob has changed orders
				if (dir->getAbilityID() != (m->unit.orders).front().ability_id) {

					// free up the directive it was previously assigned
					dir->unassignMob(m);
				}
			}
		}
	}
	*/
	
	std::unordered_set<Mob*> idle_mobs = mobH->get_idle_mobs();
	if (!idle_mobs.empty()) {
		for (auto it = idle_mobs.begin(); it != idle_mobs.end(); ) {
			auto next = std::next(it);
			if ((*it)->hasBundledDirective()) {
				Directive bundled = (*it)->popBundledDirective();
				bundled.execute(this);
			}
			else {
				(*it)->executeDefaultDirective(this);
			}
			it = next;
		}
	}

	for (Precept s : strategies) {
		if (s.checkTriggerConditions()) {
			s.execute();
		}
	}
}

void BasicSc2Bot::OnUnitCreated(const sc2::Unit* unit) {
	const sc2::ObservationInterface* observation = Observation();
	
	// mob already exists
	if (mobH->mob_exists(*unit))
		return;

	sc2::UNIT_TYPEID unit_type = unit->unit_type;
	MOB mob_type; // which category of mob to create

	// determine if unit is a structure
	bool structure = is_structure(unit);
	bool is_worker = false;
	int base_index = locH->getIndexOfClosestBase(unit->pos);

	if (!structure) {
		if (unit_type == sc2::UNIT_TYPEID::TERRAN_SCV |
			unit_type == sc2::UNIT_TYPEID::ZERG_DRONE |
			unit_type == sc2::UNIT_TYPEID::PROTOSS_PROBE) {
			is_worker = true;
			mob_type = MOB::MOB_WORKER;
		}
		else {
			mob_type = MOB::MOB_ARMY;
		}
	} else {
		if (unit_type == sc2::UNIT_TYPEID::PROTOSS_NEXUS ||
			unit_type == sc2::UNIT_TYPEID::TERRAN_COMMANDCENTER ||
			unit_type == sc2::UNIT_TYPEID::ZERG_HATCHERY) {
			mob_type = MOB::MOB_TOWNHALL;
		}
		else {
			mob_type = MOB::MOB_STRUCTURE;
			if (unit_type == sc2::UNIT_TYPEID::PROTOSS_ASSIMILATOR) {

				/*   this block of code is necessary to wake a protoss probe from standing idle             *|
				|*   while the assimilator is under construction.                                           *|
				|*   For some reason it does not trigger as idle after building this particular structure   */

				std::unordered_set<Mob*> gas_builders = mobH->filter_by_flag(mobH->get_mobs(), FLAGS::BUILDING_GAS);
				Mob* gas_builder = Directive::get_closest_to_location(gas_builders, unit->pos);
				gas_builder->remove_flag(FLAGS::BUILDING_GAS);
				Actions()->UnitCommand(&gas_builder->unit, sc2::ABILITY_ID::STOP);
			}
		}
	}
	Mob new_mob(*unit, mob_type);

	if (new_mob.unit.is_flying) {
		new_mob.set_flag(FLAGS::IS_FLYING);
	}

	new_mob.set_home_location(locH->bases[base_index].get_townhall());
	if (is_worker) {
		new_mob.set_assigned_location(new_mob.get_home_location());
		Directive directive_get_minerals_near_birth(Directive::DEFAULT_DIRECTIVE, Directive::GET_MINERALS_NEAR_LOCATION,
			new_mob.unit.unit_type, sc2::ABILITY_ID::HARVEST_GATHER, locH->bases[base_index].get_townhall());
		new_mob.assignDefaultDirective(directive_get_minerals_near_birth);
	}
	else {
		if (!structure) {
			new_mob.set_assigned_location(Directive::uniform_random_point_in_circle(new_mob.get_home_location(), 2.5F));
		}
		else {
			new_mob.set_assigned_location(new_mob.get_home_location());
		}
	}
	mobH->addMob(new_mob);	
	Mob* mob = &mobH->getMob(*unit);
	/*
	if (!is_worker && !structure) {
		Directive atk_mv_to_defense(Directive::UNIT_TYPE, Directive::NEAR_LOCATION, unit_type, sc2::ABILITY_ID::ATTACK_ATTACK, mob->get_assigned_location(), 2.0F);
		storeDirective(atk_mv_to_defense);
		Directive* dir = getLastStoredDirective();
		dir->executeForMob(this, mob);
	} */
}

void BasicSc2Bot::OnBuildingConstructionComplete(const sc2::Unit* unit) {
	Mob* mob = &mobH->getMob(*unit);
	sc2::UNIT_TYPEID unit_type = unit->unit_type;
	if (unit_type == sc2::UNIT_TYPEID::PROTOSS_NEXUS ||
		unit_type == sc2::UNIT_TYPEID::TERRAN_COMMANDCENTER ||
		unit_type == sc2::UNIT_TYPEID::ZERG_HATCHERY) {
		int base_index = locH->getIndexOfClosestBase(unit->pos);
		std::cout << "expansion " << base_index << " has been activated." << std::endl;
		locH->bases[base_index].set_active();
	}
	if (unit_type == sc2::UNIT_TYPEID::PROTOSS_PHOTONCANNON ||
		unit_type == sc2::UNIT_TYPEID::TERRAN_BUNKER ||
		unit_type == sc2::UNIT_TYPEID::TERRAN_MISSILETURRET ||
		unit_type == sc2::UNIT_TYPEID::ZERG_SPINECRAWLER ||
		unit_type == sc2::UNIT_TYPEID::ZERG_SPORECRAWLER) {
		mob->set_flag(FLAGS::IS_DEFENSE);
	}
	if (unit_type == sc2::UNIT_TYPEID::PROTOSS_ASSIMILATOR ||
		unit_type == sc2::UNIT_TYPEID::TERRAN_REFINERY ||
		unit_type == sc2::UNIT_TYPEID::ZERG_EXTRACTOR ||
		unit_type == sc2::UNIT_TYPEID::PROTOSS_ASSIMILATORRICH ||
		unit_type == sc2::UNIT_TYPEID::TERRAN_REFINERYRICH ||
		unit_type == sc2::UNIT_TYPEID::ZERG_EXTRACTORRICH) {

		// assign 3 workers to harvest this
		for (int i = 0; i < 3; i++)
			AssignNearbyWorkerToGasStructure(*unit);
	}
}

void BasicSc2Bot::OnUnitDamaged(const sc2::Unit* unit, float health, float shields) {
	const sc2::ObservationInterface* observation = Observation();
	// make Stalkers Blink away if low health
	if (unit->unit_type == sc2::UNIT_TYPEID::PROTOSS_STALKER) {
		if (have_upgrade(sc2::UPGRADE_ID::BLINKTECH)) {
			if (unit->health / unit->health_max < .5f) {
				// check if Blink is on cooldown
				if (can_unit_use_ability(*unit, sc2::ABILITY_ID::EFFECT_BLINK)) {
					Actions()->UnitCommand(unit, sc2::ABILITY_ID::EFFECT_BLINK, locH->bases[0].get_townhall());
				}
			}
		}
	}
}

void BasicSc2Bot::OnUnitIdle(const sc2::Unit* unit) {
	Mob* mob = &mobH->getMob(*unit);
	mobH->set_mob_idle(mob, true);
	if (mob->hasCurrentDirective()) {
		Directive* prev_dir = mob->getCurrentDirective();
		size_t id = prev_dir->getID();
		prev_dir->unassignMob(mob);
		mob->unassignDirective();
	}
	

}

void BasicSc2Bot::OnUnitDestroyed(const sc2::Unit* unit) {
	if (unit->alliance == sc2::Unit::Alliance::Self) {
		
		Mob* mob = &mobH->getMob(*unit);

		// if mob had directive assigned, free it up
		Directive* dir = mob->getCurrentDirective();
		if (dir) {
			mob->unassignDirective();
			dir->unassignMob(mob);
		}
		mobH->mobDeath(mob);
	}
}