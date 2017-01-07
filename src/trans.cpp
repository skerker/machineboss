#include <iomanip>
#include <fstream>
#include <set>
#include <json.hpp>

#include "trans.h"
#include "logger.h"
#include "schema.h"

using json = nlohmann::json;

TransWeight WeightAlgebra::geometricSum (const TransWeight& p) {
  return TransWeight::object ({{"/", TransWeight::array ({1, TransWeight::object ({{"-", TransWeight::array ({1,p})}})})}});
}

TransWeight WeightAlgebra::multiply (const TransWeight& l, const TransWeight& r) {
  TransWeight w;
  if (l.is_boolean() && l.get<bool>())
    w = r;
  else if (r.is_boolean() && r.get<bool>())
    w = l;
  else if (l.is_number_integer() && r.is_number_integer())
    w = l.get<int>() * r.get<int>();
  else if (l.is_number() && r.is_number())
    w = l.get<double>() * r.get<double>();
  else
    w = TransWeight::object ({{"*", TransWeight::array({l,r})}});
  return w;
}

TransWeight WeightAlgebra::add (const TransWeight& l, const TransWeight& r) {
  TransWeight w;
  if (l.is_null())
    w = r;
  else if (r.is_null())
    w = l;
  else if (l.is_number_integer() && r.is_number_integer())
    w = l.get<int>() + r.get<int>();
  else if (l.is_number() && r.is_number())
    w = l.get<double>() + r.get<double>();
  else
    w = TransWeight::object ({{"+", TransWeight::array({l,r})}});
  return w;
}

MachineTransition::MachineTransition()
{ }

MachineTransition::MachineTransition (InputSymbol in, OutputSymbol out, StateIndex dest, TransWeight weight)
  : in (in),
  out (out),
  dest (dest),
  weight (weight)
{ }

bool MachineTransition::inputEmpty() const {
  return in.empty();
}

bool MachineTransition::outputEmpty() const {
  return out.empty();
}

bool MachineTransition::isSilent() const {
  return in.empty() && out.empty();
}

bool MachineTransition::isLoud() const {
  return !in.empty() || !out.empty();
}

MachineState::MachineState()
{ }

bool MachineState::terminates() const {
  return trans.empty();
}

bool MachineState::exitsWithInput() const {
  for (const auto& t: trans)
    if (!t.inputEmpty())
      return true;
  return false;
}

bool MachineState::exitsWithoutInput() const {
  for (const auto& t: trans)
    if (t.inputEmpty())
      return true;
  return false;
}

bool MachineState::waits() const {
  return !exitsWithoutInput();
}

bool MachineState::continues() const {
  return !exitsWithInput() && !terminates();
}

bool MachineState::exitsWithIO() const {
  for (const auto& t: trans)
    if (!t.inputEmpty() || !t.outputEmpty())
      return true;
  return false;
}

bool MachineState::exitsWithoutIO() const {
  for (const auto& t: trans)
    if (t.inputEmpty() && t.outputEmpty())
      return true;
  return false;
}

bool MachineState::isSilent() const {
  return !exitsWithIO();
}

bool MachineState::isLoud() const {
  return exitsWithIO() && !exitsWithoutIO();
}

StateIndex Machine::nStates() const {
  return state.size();
}

size_t Machine::nTransitions() const {
  size_t n = 0;
  for (auto& ms: state)
    n += ms.trans.size();
  return n;
}

StateIndex Machine::startState() const {
  Assert (nStates() > 0, "Machine has no states");
  return 0;
}

StateIndex Machine::endState() const {
  Assert (nStates() > 0, "Machine has no states");
  return nStates() - 1;
}

Machine::Machine()
{ }

vguard<InputSymbol> Machine::inputAlphabet() const {
  set<InputSymbol> alph;
  for (const auto& ms: state)
    for (const auto& t: ms.trans)
      if (!t.inputEmpty())
	alph.insert (t.in);
  return vguard<InputSymbol> (alph.begin(), alph.end());
}

vguard<OutputSymbol> Machine::outputAlphabet() const {
  set<OutputSymbol> alph;
  for (const auto& ms: state)
    for (const auto& t: ms.trans)
      if (!t.outputEmpty())
	alph.insert (t.out);
  return vguard<OutputSymbol> (alph.begin(), alph.end());
}

void Machine::writeJson (ostream& out) const {
  out << "{\"state\":" << endl << " [";
  for (StateIndex s = 0; s < nStates(); ++s) {
    const MachineState& ms = state[s];
    out << (s ? "  " : "") << "{\"n\":" << s;
    if (!ms.name.is_null())
      out << "," << endl << "   \"id\":" << ms.name;
    if (ms.trans.size()) {
      out << "," << endl << "   \"trans\":[";
      size_t nt = 0;
      for (const auto& t: ms.trans) {
	if (nt++)
	  out << "," << endl << "            ";
	out << "{\"to\":" << t.dest;
	if (!t.inputEmpty()) out << ",\"in\":\"" << t.in << "\"";
	if (!t.outputEmpty()) out << ",\"out\":\"" << t.out << "\"";
	if (!(t.weight.is_boolean() && t.weight.get<bool>()))
	  out << ",\"weight\":" << t.weight;
	out << "}";
      }
      out << "]";
    }
    out << "}";
    if (s < nStates() - 1)
      out << "," << endl;
  }
  out << endl << " ]" << endl << "}" << endl;
}

string Machine::toJsonString() const {
  stringstream ss;
  writeJson (ss);
  return ss.str();
}

void Machine::readJson (istream& in) {
  state.clear();
  json pj;
  in >> pj;
  readJson(pj);
}

void Machine::readJson (const json& pj) {
  Require (MachineSchema::validate(pj), "Machine specification does not fit schema");
  json jstate = pj.at("state");
  Assert (jstate.is_array(), "state is not an array");
  map<string,StateIndex> id2n;
  for (const json& js : jstate) {
    MachineState ms;
    if (js.count("n")) {
      const StateIndex n = js.at("n").get<StateIndex>();
      Require ((StateIndex) state.size() == n, "StateIndex n=%ld out of sequence", n);
    }
    if (js.count("id")) {
      const StateName id = js.at("id");
      Assert (!id.is_number(), "id can't be a number");
      const string idStr = id.dump();
      Require (!id2n.count(idStr), "Duplicate state %s", idStr.c_str());
      id2n[idStr] = state.size();
      ms.name = id;
    }
    state.push_back (ms);
  }

  vguard<MachineState>::iterator msiter = state.begin();
  for (const json& js : jstate) {
    MachineState& ms = *msiter++;
    if (js.count ("trans")) {
      const json& jtrans = js.at("trans");
      Assert (jtrans.is_array(), "trans is not an array");
      for (const json& jt : jtrans) {
	MachineTransition t;
	const json& dest = jt.at("to");
	t.dest = dest.is_number()
	  ? dest.get<StateIndex>()
	  : id2n.at (dest.dump());
	if (jt.count("in"))
	  t.in = jt.at("in").get<string>();
	if (jt.count("out"))
	  t.out = jt.at("out").get<string>();
	t.weight = jt.count("weight") ? jt.at("weight") : TransWeight(true);
	ms.trans.push_back (t);
      }
    }
  }
}

Machine Machine::fromJson (istream& in) {
  Machine machine;
  machine.readJson (in);
  return machine;
}

Machine Machine::fromFile (const char* filename) {
  ifstream infile (filename);
  if (!infile)
    Fail ("File not found: %s", filename);
  return fromJson (infile);
}

bool Machine::isErgodicMachine() const {
  return accessibleStates().size() == nStates();
}

bool Machine::isWaitingMachine() const {
  for (const auto& ms: state)
    if (!ms.waits() && !ms.continues())
      return false;
  return true;
}

bool Machine::isAdvancingMachine() const {
  for (StateIndex s = 1; s < nStates(); ++s)
    for (const auto& t: state[s].trans)
      if (t.isSilent() && t.dest <= s)
	return false;
  return true;
}

Machine Machine::compose (const Machine& first, const Machine& origSecond) {
  LogThisAt(3,"Composing " << first.nStates() << "-state transducer with " << origSecond.nStates() << "-state transducer" << endl);
  const Machine second = origSecond.isWaitingMachine() ? origSecond : origSecond.waitingMachine();
  Assert (second.isWaitingMachine(), "Attempt to compose transducers A*B where B is not a waiting machine");

  Machine compMachine;
  vguard<MachineState>& comp = compMachine.state;
  comp = vguard<MachineState> (first.nStates() * second.nStates());

  auto compState = [&](StateIndex i,StateIndex j) -> StateIndex {
    return i * second.nStates() + j;
  };
  for (StateIndex i = 0; i < first.nStates(); ++i)
    for (StateIndex j = 0; j < second.nStates(); ++j) {
      MachineState& ms = comp[compState(i,j)];
      ms.name = StateName ({first.state[i].name, second.state[j].name});
    }
  for (StateIndex i = 0; i < first.nStates(); ++i)
    for (StateIndex j = 0; j < second.nStates(); ++j) {
      MachineState& ms = comp[compState(i,j)];
      const MachineState& msi = first.state[i];
      const MachineState& msj = second.state[j];
      TransAccumulator ta;
      if (msj.waits() || msj.terminates()) {
	for (const auto& it: msi.trans)
	  if (it.outputEmpty())
	    ta.accumulate (it.in, string(), compState(it.dest,j), it.weight);
	  else
	    for (const auto& jt: msj.trans)
	      if (it.out == jt.in)
		ta.accumulate (it.in, jt.out, compState(it.dest,jt.dest), WeightAlgebra::multiply (it.weight, jt.weight));
      } else
	for (const auto& jt: msj.trans)
	  ta.accumulate (string(), jt.out, compState(i,jt.dest), jt.weight);
      ms.trans = ta.transitions();
    }

  LogThisAt(3,"Transducer composition yielded " << compMachine.nStates() << "-state machine" << endl);
  return compMachine.ergodicMachine().advancingMachine();
}

set<StateIndex> Machine::accessibleStates() const {
  vguard<bool> reachableFromStart (nStates(), false);
  deque<StateIndex> fwdQueue;
  fwdQueue.push_back (startState());
  reachableFromStart[fwdQueue.front()] = true;
  while (fwdQueue.size()) {
    const StateIndex c = fwdQueue.front();
    fwdQueue.pop_front();
    for (const auto& t: state[c].trans)
      if (!reachableFromStart[t.dest]) {
	reachableFromStart[t.dest] = true;
	fwdQueue.push_back (t.dest);
      }
  }

  vguard<bool> endReachableFrom (nStates(), false);
  vguard<vguard<StateIndex> > sources (nStates());
  deque<StateIndex> backQueue;
  for (StateIndex s = 0; s < nStates(); ++s)
    for (const auto& t: state[s].trans)
      sources[t.dest].push_back (s);
  backQueue.push_back (endState());
  endReachableFrom[backQueue.front()] = true;
  while (backQueue.size()) {
    const StateIndex c = backQueue.front();
    backQueue.pop_front();
    for (StateIndex src: sources[c])
      if (!endReachableFrom[src]) {
	endReachableFrom[src] = true;
	backQueue.push_back (src);
      }
  }

  set<StateIndex> as;
  for (StateIndex s = 0; s < nStates(); ++s)
    if (reachableFromStart[s] && endReachableFrom[s])
      as.insert (s);

  return as;
}

Machine Machine::ergodicMachine() const {
  vguard<bool> keep (nStates(), false);
  for (StateIndex s : accessibleStates())
    keep[s] = true;

  map<StateIndex,StateIndex> nullEquiv;
  for (StateIndex s = 0; s < nStates(); ++s)
    if (keep[s]) {
      StateIndex d = s;
      while (state[d].trans.size() == 1 && state[d].trans.front().isSilent())
	d = state[d].trans.front().dest;
      if (d != s)
	nullEquiv[s] = d;
    }
  vguard<StateIndex> old2new (nStates());
  StateIndex ns = 0;
  for (StateIndex oldIdx = 0; oldIdx < nStates(); ++oldIdx)
    if (keep[oldIdx] && !nullEquiv.count(oldIdx))
      old2new[oldIdx] = ns++;
  for (StateIndex oldIdx = 0; oldIdx < nStates(); ++oldIdx)
    if (keep[oldIdx] && nullEquiv.count(oldIdx))
      old2new[oldIdx] = old2new[nullEquiv.at(oldIdx)];

  Machine em;
  em.state.reserve (ns);
  for (StateIndex oldIdx = 0; oldIdx < nStates(); ++oldIdx)
    if (keep[oldIdx] && !nullEquiv.count(oldIdx)) {
      em.state.push_back (MachineState());
      em.state.back().name = state[oldIdx].name;
      for (auto& t: state[oldIdx].trans)
	if (keep[t.dest])
	  em.state.back().trans.push_back (MachineTransition (t.in, t.out, old2new.at(t.dest), t.weight));
    }

  Assert (em.isErgodicMachine(), "failed to create ergodic machine");
  LogThisAt(5,"Trimmed " << nStates() << "-state transducer into " << em.nStates() << "-state ergodic machine" << endl);
  LogThisAt(7,em.toJsonString() << endl);

  return em;
}

Machine Machine::waitingMachine() const {
  vguard<MachineState> newState (state);
  vguard<StateIndex> old2new (nStates()), new2old;
  for (StateIndex s = 0; s < nStates(); ++s) {
    const MachineState& ms = state[s];
    old2new[s] = new2old.size();
    new2old.push_back (s);
    if (!ms.waits() && !ms.continues()) {
      MachineState c, w;
      c.name = ms.name;
      w.name[MachineWaitTag] = ms.name;
      for (const auto& t: ms.trans)
	if (t.inputEmpty())
	  c.trans.push_back(t);
	else
	  w.trans.push_back(t);
      c.trans.push_back (MachineTransition (string(), string(), newState.size(), TransWeight(true)));
      old2new.push_back (new2old.size());
      new2old.push_back (newState.size());
      swap (newState[s], c);
      newState.push_back (w);
    }
  }
  Machine wm;
  for (StateIndex s: new2old) {
    MachineState& ms = newState[s];
    for (auto& t: ms.trans)
      t.dest = old2new[t.dest];
    wm.state.push_back (ms);
  }
  Assert (wm.isWaitingMachine(), "failed to create waiting machine");
  LogThisAt(5,"Converted " << nStates() << "-state transducer into " << wm.nStates() << "-state waiting machine" << endl);
  LogThisAt(7,wm.toJsonString() << endl);
  return wm;
}

Machine Machine::advancingMachine() const {
  Machine am;
  if (nStates()) {
    am.state.reserve (nStates());
    vguard<TransList> effTrans;
    vguard<StateIndex> minDest (nStates());
    for (StateIndex s = 0; s < nStates(); ++s) {
      const MachineState& ms = state[s];
      effTrans.push_back (ms.trans);
      am.state.push_back (MachineState());
      MachineState& ams = am.state.back();
      ams.name = ms.name;
      vguard<bool> markedForUpdate (s + 1, false);
      function<void(StateIndex)> markForUpdate = [&](StateIndex i) {
	markedForUpdate[i] = true;
	if (minDest[i] < s)
	  for (auto& t_ij: effTrans[i]) {
	    Assert (t_ij.isLoud() || i == s || t_ij.dest > i, "oops: cycle. i=%d j=%d", i, t_ij.dest);
	    if (t_ij.isSilent() && t_ij.dest < s && !markedForUpdate[t_ij.dest])
	      markForUpdate (t_ij.dest);
	  }
      };
      markForUpdate(s);
      function<void(StateIndex)> updateEffTrans = [&](StateIndex i) {
	TransList newEffTrans, elimTrans;
	StateIndex newMinDest = nStates();
	for (auto& t_ij: effTrans[i]) {
	  if (t_ij.isLoud())
	    newEffTrans.push_back(t_ij);
	  else {
	    const StateIndex j = t_ij.dest;
	    if (j >= s) {
	      newMinDest = min (newMinDest, j);
	      newEffTrans.push_back(t_ij);
	    } else
	      for (auto& t_jk: effTrans[j]) {
		const StateIndex k = t_jk.dest;
		Assert (t_jk.isLoud() || (k>j && (k > i || (k == i && i == s))), "oops: cycle. i=%d j=%d k=%d", i, j, k);
		newMinDest = min (newMinDest, k);
		newEffTrans.push_back (MachineTransition (t_jk.in, t_jk.out, k, WeightAlgebra::multiply (t_ij.weight, t_jk.weight)));
	      }
	  }
	}
	minDest[i] = newMinDest;
	effTrans[i] = newEffTrans;
      };
      for (StateIndex t = s; t > 0; --t)
	updateEffTrans(t-1);
      updateEffTrans(s);
      // aggregate all transitions that go to the same place
      TransAccumulator ta;
      for (const auto& t: effTrans[s])
	ta.accumulate (t.in, t.out, t.dest, t.weight);
      const auto et = ta.transitions();
      // factor out self-loops
      TransWeight exitSelf (true);
      for (const auto& t: et)
	if (t.isSilent() && t.dest == s)
	  exitSelf = WeightAlgebra::geometricSum (t.weight);
	else
	  ams.trans.push_back (t);
      if (!(exitSelf.is_boolean() && exitSelf.get<bool>()))
	for (auto& t: ams.trans)
	  t.weight = WeightAlgebra::multiply (exitSelf, t.weight);
      effTrans[s] = ams.trans;
    }
    Assert (am.isAdvancingMachine(), "failed to create advancing machine");
    LogThisAt(5,"Converted " << nTransitions() << "-transition transducer into " << am.nTransitions() << "-transition advancing machine" << endl);
    LogThisAt(7,am.toJsonString() << endl);
  }
  return am;
}

void TransAccumulator::accumulate (InputSymbol in, OutputSymbol out, StateIndex dest, TransWeight w) {
  if (t[dest][in].count(out))
    t[dest][in][out] = WeightAlgebra::add(w,t[dest][in][out]);
  else
    t[dest][in][out] = w;
}

TransList TransAccumulator::transitions() const {
  TransList trans;
  for (const auto& dest_map: t)
    for (const auto& in_map: dest_map.second)
      for (const auto& out_weight: in_map.second)
	trans.push_back (MachineTransition (in_map.first, out_weight.first, dest_map.first, out_weight.second));
  return trans;
}
