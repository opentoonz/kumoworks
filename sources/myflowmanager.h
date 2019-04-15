#pragma once

#ifndef MYFLOWMANAGER_H
#define MYFLOWMANAGER_H

#include <QList>
#include <QMap>

enum FlowId {
  Flow_Silhouette2Voxel = 0,
  Flow_ArrangeMetaballs,
  Flow_GenerateSimplexNoise,
  Flow_GenerateWorlyNoise,
  Flow_CompositeNoises,
  Flow_CompositeMetaballAndNoise,
  Flow_RayMarchingRender,
  Flow_RenderReflection,
  Flow_GetCloudImage,
  Flow_GenerateSky,
  Flow_GetBackgroundImage,
  Flow_CombineBackground,
  Flow_GetLayoutImage,
  Flow_GetSunRectOnViewer,
  Flow_GetUniqueSunRectOnViewer,
  Flow_GetEyeLevelPosOnViewer,
  Flow_Amount
};

const FlowId Flow_None = Flow_Amount;

class Flow {
  bool m_done = false;
  QList<Flow*> m_subsequentFlows;
public:
  bool isDone() { return m_done; }
  void setUndone() { 
    m_done = false;
    for (Flow* f : m_subsequentFlows) {
      f->setUndone();
    }
  }
  void setDone() { m_done = true; }

  void addSubSequentFlow(Flow* flow) {
    m_subsequentFlows.append(flow);
  }
};

class FlowManager {
  QMap<FlowId, Flow*> m_flows;
public:
  
  FlowManager() {
    for(int fid = 0; fid < (int)Flow_Amount; fid++)
      m_flows[(FlowId)fid] = new Flow();

    // define flow connections
    getFlow(Flow_Silhouette2Voxel)->addSubSequentFlow(getFlow(Flow_ArrangeMetaballs));
    getFlow(Flow_Silhouette2Voxel)->addSubSequentFlow(getFlow(Flow_GenerateSimplexNoise));
    getFlow(Flow_Silhouette2Voxel)->addSubSequentFlow(getFlow(Flow_GenerateWorlyNoise));
    getFlow(Flow_ArrangeMetaballs)->addSubSequentFlow(getFlow(Flow_CompositeMetaballAndNoise));
    getFlow(Flow_CompositeMetaballAndNoise)->addSubSequentFlow(getFlow(Flow_RayMarchingRender));
    getFlow(Flow_GenerateSimplexNoise)->addSubSequentFlow(getFlow(Flow_CompositeNoises));
    getFlow(Flow_GenerateWorlyNoise)->addSubSequentFlow(getFlow(Flow_CompositeNoises));
    getFlow(Flow_CompositeNoises)->addSubSequentFlow(getFlow(Flow_CompositeMetaballAndNoise));
    getFlow(Flow_RayMarchingRender)->addSubSequentFlow(getFlow(Flow_RenderReflection));
    getFlow(Flow_RenderReflection)->addSubSequentFlow(getFlow(Flow_GetCloudImage));
    getFlow(Flow_GenerateSky)->addSubSequentFlow(getFlow(Flow_CombineBackground));
    getFlow(Flow_GetBackgroundImage)->addSubSequentFlow(getFlow(Flow_CombineBackground));
    getFlow(Flow_CombineBackground)->addSubSequentFlow(getFlow(Flow_GetCloudImage));
  }

  bool isDone(FlowId id) {
    Flow* flow = m_flows.value(id);
    if (!flow) return false;
    return flow->isDone();
  }

  void setDone(FlowId id) {
    Flow* flow = m_flows.value(id);
    if (flow) flow->setDone();
  }
  void setUndone(FlowId id) {
    Flow* flow = m_flows.value(id);
    if (flow) flow->setUndone();
  }

  ~FlowManager() {
    for (Flow* f : m_flows)
      delete f;
  }

  Flow* getFlow(FlowId id) {
    return m_flows.value(id);
  }
};


#endif
