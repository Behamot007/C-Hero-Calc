#include "inputProcessing.h"

using namespace std;

// Length of timestamp in debug messages
const size_t FINISH_MESSAGE_LENGTH = 20;

IOManager::IOManager() {
    this->useMacroFile = false;
    this->showQueries = true;
    this->outputLevel = BASIC_OUTPUT;
    
    this->lastTimedOutput = -1;
}

bool IOManager::shouldOutput(OutputLevel urgency) {
    return (this->outputLevel >= urgency);
}

// Initialize a macro file provided by filename
void IOManager::initMacroFile(string macroFileName, bool showInput) {
    this->macroFile.open(macroFileName);
    
    this->useMacroFile = this->macroFile.good();
    this->showQueries = this->macroFile.good() && showInput;
    if (!this->macroFile.good()) {
        cout << "Could not find Macro File. Switching to Manual Input." << endl;
    }
}

// Output method called only by class. takes an output level to determine if it should be printed or not
void IOManager::printBuffer(OutputLevel urgency) {
    if (this->shouldOutput(urgency)) {
        cout << this->outputStream.str();
    } 
    this->outputStream.str("");
    this->outputStream.clear();
}

// Returns the correct amount of spaces for indentation
string IOManager::getIndent(int indent) {
    return string(indent * INDENT_WIDTH, ' ');
}

// Output simple message
void IOManager::outputMessage(string message, OutputLevel urgency, int indent, bool linebreak) {
    this->outputStream << this->getIndent(indent) + message;
    if (linebreak) {
        this->outputStream << endl;
    }
    this->printBuffer(urgency);
}

// Output message that will be terminated by a timestamp by the next timed message
void IOManager::timedOutput(string message, OutputLevel urgency, int indent, bool reset) {
    if (this->lastTimedOutput >= 0 && !reset) {
        this->finishTimedOutput(urgency);
    }
    lastTimedOutput = time(NULL);
    this->outputStream << left << setw(STANDARD_CMD_WIDTH - FINISH_MESSAGE_LENGTH) << this->getIndent(indent) + message;
    this->printBuffer(urgency);
}

// Finish the final timed message without adding another
void IOManager::finishTimedOutput(OutputLevel urgency) {
    this->outputStream << "Done! (" << right << setw(3) << time(NULL) - this->lastTimedOutput << " seconds)" << endl; // Exactly 20 characters long
    this->printBuffer(urgency);
}

// Stop timed messages for a time. used to have properly formatted output when outputting substeps
void IOManager::suspendTimedOutputs(OutputLevel urgency) {
    this->outputStream << endl;
    this->printBuffer(urgency);
}

// Start timed outputs again.
void IOManager::resumeTimedOutputs(OutputLevel urgency) {
    this->outputStream << left << setw(STANDARD_CMD_WIDTH - FINISH_MESSAGE_LENGTH) << "";
    this->printBuffer(urgency);
}

// Wait for user input before continuing. Used to stop program from closing outside of a command line.
void IOManager::haltExecution() {
    if (this->shouldOutput(CMD_OUTPUT)) {
        cout << "Press enter to exit...";
        cin.get();
    }
}

// Method for handling ALL input. Gives access to help, error resistance and macro file for input.
string IOManager::getResistantInput(string query, string help, QueryType queryType) {
    string inputString;
    string firstToken;
    while (true) {
        // Check first if there is still a line in the macro file
        if (this->useMacroFile) {
            this->useMacroFile = (bool) getline(this->macroFile, inputString);
        } 
        // Print the query only if no macro file is used or specifically asked for
        if (!this->useMacroFile || this->showQueries) {
            cout << query;
        }
        // Ask for user input
        if (!this->useMacroFile) {
            getline(cin, inputString);
        }
        
        // Process Input
        inputString = split(toLower(inputString), COMMENT_DELIMITOR)[0]; // trim potential comments in a macrofile and convert to lowercase
        firstToken = split(inputString, TOKEN_SEPARATOR)[0]; // except for rare input only the first string till a space is used
        if (this->useMacroFile && this->showQueries) {
            cout << inputString << endl; // Show input if a macro file is used
        }
        if (firstToken == "help") {
            cout << help;
        } else {
            if (queryType == question && (firstToken == POSITIVE_ANSWER || firstToken == NEGATIVE_ANSWER)) {
                return firstToken;
            }
            if (queryType == integer) {
                try {
                    stoi(firstToken);
                    return firstToken;
                } catch (const exception & e) {}
            }
            if (queryType == raw) {
                return inputString;
            }
            if (queryType == rawFirst) {
                return firstToken;
            }
        }
    }
}

// Ask the user a question that they can answer via command line
bool IOManager::askYesNoQuestion(string questionMessage, string help, OutputLevel urgency, string defaultAnswer) {
    string inputString;
    if (!this->shouldOutput(urgency)) {
        inputString = defaultAnswer;
    } else {
        inputString = this->getResistantInput(questionMessage + " (" + POSITIVE_ANSWER + "/" + NEGATIVE_ANSWER + "): ", help, question);
    }
    
    if (inputString == NEGATIVE_ANSWER) {
        return false;
    }
    if (inputString == POSITIVE_ANSWER) {
        return true;
    }
    return false;
}

// Promt the User via command line to input his hero levels and return a vector of their indices
vector<int8_t> IOManager::takeHerolevelInput() {
    vector<int8_t> heroes {};
    string input;
    pair<Monster, int> heroData;
    
    if (!this->useMacroFile || this->showQueries) {
        cout << endl << "Enter your Heroes with levels. Press enter after every Hero." << endl;
        cout << "Press enter twice or type done to proceed without inputting additional Heroes." << endl;
    }
    int cancelCounter = 0;
    do {
        input = this->getResistantInput("Enter Hero " + to_string(heroes.size()+1) + ": ", heroInputHelp, rawFirst);
        if (input == "") {
            cancelCounter++;
        } else {
            cancelCounter = 0;
            try {
                heroData = parseHeroString(input);
                heroes.push_back(addLeveledHero(heroData.first, heroData.second));
            } catch (const exception & e) {};
        }
    } while (input != "done" && cancelCounter < 2);
    
    return heroes;
}

// Promts the user to input instance(s) to be solved 
vector<Instance> IOManager::takeInstanceInput(string prompt) {
    vector<Instance> instances;
    vector<string> instanceStrings;
    
    string input;
    
    while (true) {
        input = this->getResistantInput(prompt, lineupInputHelp, raw);
        instances.clear();
        instanceStrings = split(input, TOKEN_SEPARATOR);
        try {
            for (size_t i = 0; i < instanceStrings.size(); i++) {
                instances.push_back(makeInstanceFromString(instanceStrings[i]));
            }
            return instances;
        } catch (const exception & e) {}
    }
}

// Convert a lineup string into an actual instance to solve
Instance makeInstanceFromString(string instanceString) {
    Instance instance;
    int dashPosition = (int) instanceString.find(QUEST_NUMBER_SEPARTOR);
    
    if (instanceString.compare(0, QUEST_PREFIX.length(), QUEST_PREFIX) == 0) {
        int questNumber = stoi(instanceString.substr(QUEST_PREFIX.length(), dashPosition-QUEST_PREFIX.length()));
        instance.target = makeArmyFromStrings(quests[questNumber]);
        instance.maxCombatants = ARMY_MAX_SIZE - (stoi(instanceString.substr(dashPosition+1, 1)) - 1);
    } else {
        vector<string> stringLineup = split(instanceString, ELEMENT_SEPARATOR);
        instance.target = makeArmyFromStrings(stringLineup);
        instance.maxCombatants = ARMY_MAX_SIZE;
    }
    instance.targetSize = instance.target.monsterAmount;
    return instance;
}

// Parse string linup input into actual monsters. If there are heroes in the input, a leveled hero is added to the database
Army makeArmyFromStrings(vector<string> stringMonsters) {
    Army army;
    pair<Monster, int> heroData;
    
    for(size_t i = 0; i < stringMonsters.size(); i++) {
        if(stringMonsters[i].find(HEROLEVEL_SEPARATOR()) != stringMonsters[i].npos) {
            heroData = parseHeroString(stringMonsters[i]);
            army.add(addLeveledHero(heroData.first, heroData.second));
        } else {
            army.add(monsterMap.at(stringMonsters[i]));
        }
    }
    return army;
}

// Parse hero input from a string into its name and level
pair<Monster, int> parseHeroString(string heroString) {
    string name = heroString.substr(0, heroString.find(HEROLEVEL_SEPARATOR()));
    int level = stoi(heroString.substr(heroString.find(HEROLEVEL_SEPARATOR())+1));
    
	Monster hero;
	for (size_t i = 0; i < baseHeroes.size(); i++) {
		if (baseHeroes[i].baseName == name) {
            return pair<Monster, int>(baseHeroes[i], level);
		}
	}
    throw out_of_range("Hero Name Not Found");
}

// Create valid string to be used ingame to view the battle between armies friendly and hostile
string makeBattleReplay(Army friendly, Army hostile) {
    stringstream replay;
    replay << "{";
        replay << "\"winner\""  << ":" << "\"Unknown\"" << ",";
        replay << "\"left\""    << ":" << "\"Solution\"" << ",";
        replay << "\"right\""   << ":" << "\"Instance\"" << ",";
        replay << "\"date\""    << ":" << time(NULL) << ",";
        replay << "\"title\""   << ":" << "\"Proposed Solution\"" << ",";
        replay << "\"setup\""   << ":" << getReplaySetup(friendly) << ",";
        replay << "\"shero\""   << ":" << getReplayHeroes(friendly) << ",";
        replay << "\"player\""  << ":" << getReplaySetup(hostile) << ",";
        replay << "\"phero\""   << ":" << getReplayHeroes(hostile);
    replay << "}";
    string unencoded = replay.str();
    return base64_encode((const unsigned char*) unencoded.c_str(), (int) unencoded.size());
}

// Get lineup in ingame indices
string getReplaySetup(Army setup) {
    stringstream stringSetup;
    size_t i;
    stringSetup << "[";
    for (i = 0; i < ARMY_MAX_SIZE * TOURNAMENT_LINES; i++) {
        if ((int) (i % ARMY_MAX_SIZE) < setup.monsterAmount) {
            stringSetup << getReplayMonsterNumber(monsterReference[setup.monsters[setup.monsterAmount - (i % ARMY_MAX_SIZE) - 1]]);
        } else {
            stringSetup << to_string(REPLAY_EMPTY_SPOT);
        }
        if (i < ARMY_MAX_SIZE * TOURNAMENT_LINES - 1) {
            stringSetup << ",";
        }
    }
    stringSetup << "]";
    return stringSetup.str();
}

// Get ingame index of monster. 0 and Positive for monsters and -1 to negative for heroes
string getReplayMonsterNumber(Monster monster) {
    int8_t index = REPLAY_EMPTY_SPOT;
    size_t i;
    if(monster.rarity != NO_HERO) {
        for (i = 0; i < baseHeroes.size(); i++) {
            if (baseHeroes[i].baseName == monster.baseName) {
                index = (int8_t) (-i - 2);
            }
        }
    } else {
        for (i = 0; i < monsterBaseList.size(); i++) {
            if (monster.name == monsterBaseList[i].name) {
                index = (int8_t) i;
            }
        }
    }
    return to_string(index);
}

// Get list of relevant herolevels in ingame format
string getReplayHeroes(Army setup) {
    stringstream heroes;
    Monster monster;
    int level;
    heroes << "[";
    for (size_t i = 0; i < baseHeroes.size(); i++) {
        level = 0;
        for (int j = 0; j < setup.monsterAmount; j++) {
            monster = monsterReference[setup.monsters[j]];
            if (monster.rarity != NO_HERO && monster.baseName == baseHeroes[i].baseName) {
                level = monster.level;
                break;
            }
        }
        heroes << level;
        if (i < baseHeroes.size()-1) {
            heroes << ",";
        }
    }
    heroes << "]";
    return heroes.str();
}

string Instance::toJSON() {
    stringstream s;
    s << "{";
        s << "\"target\""  << ":" << this->target.toJSON() << ",";
        s << "\"solution\""  << ":" << this->bestSolution.toJSON() << ",";
        s << "\"time\""  << ":" << this->calculationTime << ",";
        s << "\"fights\"" << ":" << this->totalFightsSimulated << ",";
        s << "\"replay\"" << ":" << "\"" << makeBattleReplay(this->bestSolution, this->target) << "\"";
    s << "}";
    return s.str();
}

string Instance::toString() {
    stringstream s;
        
    s << endl << "Solution for " << this->target.toString() << ":" << endl;
    // Announce the result
    if (!this->bestSolution.isEmpty()) {
        s << "  " << this->bestSolution.toString() << endl;
    } else {
        s << endl << "Could not find a solution that beats this lineup." << endl;
    }
    s << "  " << this->totalFightsSimulated << " Fights simulated." << endl;
    s << "  Total Calculation Time: " << this->calculationTime << endl << endl;
    if (!this->bestSolution.isEmpty()) {
        s << "Battle Replay (Use on Ingame Tournament Page):" << endl << makeBattleReplay(this->bestSolution, this->target) << endl << endl;
    }
    
    return s.str();
}

// Splits strings into a vector of strings.
vector<string> split(string target, string separator) {
    vector<string> output;
    string substring;
    size_t start = 0;
    size_t occurrence = 0;
    while(occurrence != target.npos) {
        occurrence = target.find(separator, start);
        substring = target.substr(start, occurrence-start);
        if (start == 0 || substring.length() > 0) {
            output.push_back(substring);
        }
        start = occurrence + separator.length();
    }
    return output;
}

// Convert a string to lowercase where available
string toLower(string input) {
    for (size_t i = 0; i < input.length(); i++) {
        input[i] = tolower(input[i], locale());
    }
    return input;
}