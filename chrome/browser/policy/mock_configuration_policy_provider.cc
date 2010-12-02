// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/mock_configuration_policy_provider.h"

#include "chrome/browser/policy/configuration_policy_pref_store.h"

namespace policy {

MockConfigurationPolicyProvider::MockConfigurationPolicyProvider()
    : ConfigurationPolicyProvider(
        ConfigurationPolicyPrefStore::GetChromePolicyDefinitionList()) {
}

MockConfigurationPolicyProvider::~MockConfigurationPolicyProvider() {
  STLDeleteValues(&policy_map_);
}

void MockConfigurationPolicyProvider::AddPolicy(ConfigurationPolicyType policy,
                                                Value* value) {
  std::swap(policy_map_[policy], value);
  delete value;
}

bool MockConfigurationPolicyProvider::Provide(
    ConfigurationPolicyStoreInterface* store) {
  for (PolicyMap::const_iterator current = policy_map_.begin();
       current != policy_map_.end(); ++current) {
    store->Apply(current->first, current->second->DeepCopy());
  }
  return true;
}

}