// src/directorates/knowledge-authority.ts

export interface GraphEdge {
  source: string;
  target: string;
  relation: string;
}

export class KnowledgeAuthority {
  private edges: GraphEdge[] = [];

  public addRelation(source: string, target: string, relation: string): void {
    const exists = this.edges.some(e => e.source === source && e.target === target && e.relation === relation);
    if (!exists) {
      this.edges.push({ source, target, relation });
    }
  }

  public getRelationsForNode(node: string): GraphEdge[] {
    return this.edges.filter(e => e.source === node || e.target === node);
  }

  public getGraphEdgeCount(): number {
    return this.edges.length;
  }
}
