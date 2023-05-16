
#include<iostream>
#include<cstring>
#include<cstdio>
#include<map>
#include<vector>
#include<string>
#include<ctime>
#include<cmath>
#include<cstdlib>
using namespace std;


#define pi 3.1415926535897932384626433832795

bool L1_flag=1;

//normal distribution
double rand(double min, double max)
{
    return min+(max-min)*rand()/(RAND_MAX+1.0);
}
double normal(double x, double miu,double sigma)
{
    return 1.0/sqrt(2*pi)/sigma*exp(-1*(x-miu)*(x-miu)/(2*sigma*sigma));
}
double randn(double miu,double sigma, double min ,double max)
{
    double x,y,dScope;
    do{
        x=rand(min,max);
        y=normal(x,miu,sigma);
        dScope=rand(0.0,normal(miu,miu,sigma));
    }while(dScope>y);
    return x;
}

double sqr(double x)
{
    return x*x;
}

double vec_len(vector<double> &a)
{
	double res=0;
    for (int i=0; i<a.size(); i++)
		res+=a[i]*a[i];
	res = sqrt(res);
	return res;
}

string version;
char buf[100000],buf1[100000];
int relation_num,entity_num;
map<string,int> relation2id,entity2id;    // dict.全局map
map<int,string> id2entity,id2relation;


map<int,map<int,int> > left_entity,right_entity; // left_entity[r]={h:count}   每种关系r,每个h的出现次数
map<int,double> left_num,right_num;

class Train{

public:
	map<pair<int,int>, map<int,int> > ok;  
    void add(int x,int y,int z)            // 文件中解析得到的每个样本：h,t,r
    {
        fb_h.push_back(x);
        fb_r.push_back(z);
        fb_l.push_back(y);
        ok[make_pair(x,z)][y]=1;    // 查找<h,r>对应的[t]是否存在. 保证负样本采样一个随机t后，对应的样本(h,r,t)存在,是强负样本
    }

    void run(int n_in,double rate_in,double margin_in,int method_in)
    {
        n = n_in;                                            // 传入的k。
        rate = rate_in;
        margin = margin_in;
        method = method_in;
        relation_vec.resize(relation_num);          // 根据传入的size,resize模型参数vec
		for (int i=0; i<relation_vec.size(); i++)
			relation_vec[i].resize(n);  

        entity_vec.resize(entity_num);              // resize成|E|个。 每个resize成k
		for (int i=0; i<entity_vec.size(); i++)
			entity_vec[i].resize(n);

        relation_tmp.resize(relation_num);
		for (int i=0; i<relation_tmp.size(); i++)
			relation_tmp[i].resize(n);
        entity_tmp.resize(entity_num);
		for (int i=0; i<entity_tmp.size(); i++)
			entity_tmp[i].resize(n);

        // 初始化
        for (int i=0; i<relation_num; i++)
        {
            for (int ii=0; ii<n; ii++)
                relation_vec[i][ii] = randn(0,1.0/n,-6/sqrt(n),6/sqrt(n));
        }
        for (int i=0; i<entity_num; i++)
        {
            for (int ii=0; ii<n; ii++)
                entity_vec[i][ii] = randn(0,1.0/n,-6/sqrt(n),6/sqrt(n));
            norm(entity_vec[i]);
        }


        bfgs();
    }

private:
    int n,method;  // k
    double res;    // loss
    double count,count1;//loss function gradient
    double rate,margin;
    double belta;
    vector<int> fb_h,fb_l,fb_r;                          // 所有样本：h,t,r (n_triple个)
    vector<vector<double> > relation_vec,entity_vec;     // 模型参数. vec[id]=vector_embed
    vector<vector<double> > relation_tmp,entity_tmp;
    double norm(vector<double> &a)
    {
        double x = vec_len(a);
        if (x>1)
        for (int ii=0; ii<a.size(); ii++)
                a[ii]/=x;
        return 0;
    }
    int rand_max(int x)
    {
        int res = (rand()*rand())%x;
        while (res<0)
            res+=x;
        return res;
    }

    void bfgs()
    {
        res=0;
        int nbatches=100;
        int nepoch = 1000;
        int batchsize = fb_h.size()/nbatches;
            for (int epoch=0; epoch<nepoch; epoch++)
            {

            	res=0;
             	for (int batch = 0; batch<nbatches; batch++)  // 每个batch
             	{
             		relation_tmp=relation_vec;
            		entity_tmp = entity_vec;
             		for (int k=0; k<batchsize; k++)           // 每次抽一个样本（和对应的负样本）。抽B次
             		{
						int i=rand_max(fb_h.size());
						int j=rand_max(entity_num);
						double pr = 1000*right_num[fb_r[i]]/(right_num[fb_r[i]]+left_num[fb_r[i]]);
						if (method ==0)
                            pr = 500;
						if (rand()%1000<pr)
						{
							while (ok[make_pair(fb_h[i],fb_r[i])].count(j)>0) // 抽一个存在的t
								j=rand_max(entity_num);
                            // 训练h[i],t[i].r[i], 和对应的负样本，t换了
							train_kb(fb_h[i],fb_l[i],fb_r[i],fb_h[i],j,fb_r[i]);
						}
						else
						{
							while (ok[make_pair(j,fb_r[i])].count(fb_l[i])>0)
								j=rand_max(entity_num);
                            // 训练h[i],t[i].r[i], 和对应的负样本，h换了
							train_kb(fb_h[i],fb_l[i],fb_r[i],j,fb_l[i],fb_r[i]);
						}
                		norm(relation_tmp[fb_r[i]]); // 训练过的样本，涉及到的4个embedding，归一化
                		norm(entity_tmp[fb_h[i]]);
                		norm(entity_tmp[fb_l[i]]);
                		norm(entity_tmp[j]);
             		}
		            relation_vec = relation_tmp;
		            entity_vec = entity_tmp;
             	}
                cout<<"epoch:"<<epoch<<' '<<res<<endl;
                FILE* f2 = fopen(("relation2vec."+version).c_str(),"w"); // 每个epoch, checkpoint,写入最终的模型参数
                FILE* f3 = fopen(("entity2vec."+version).c_str(),"w");
                for (int i=0; i<relation_num; i++)
                {
                    for (int ii=0; ii<n; ii++)
                        fprintf(f2,"%.6lf\t",relation_vec[i][ii]);    // r_embed: \t分割
                    fprintf(f2,"\n");
                }
                for (int i=0; i<entity_num; i++)
                {
                    for (int ii=0; ii<n; ii++)
                        fprintf(f3,"%.6lf\t",entity_vec[i][ii]);      // e_embed: \t分割
                    fprintf(f3,"\n");
                }
                fclose(f2);
                fclose(f3);
            }
    }
    double res1;
    double calc_sum(int e1,int e2,int rel)
    {
        double sum=0;
        if (L1_flag)
        	for (int ii=0; ii<n; ii++)
            	sum+=fabs(entity_vec[e2][ii]-entity_vec[e1][ii]-relation_vec[rel][ii]);
        else
        	for (int ii=0; ii<n; ii++)
            	sum+=sqr(entity_vec[e2][ii]-entity_vec[e1][ii]-relation_vec[rel][ii]);
        return sum;
    }

    void gradient(int e1_a,int e2_a,int rel_a,int e1_b,int e2_b,int rel_b)
    {
        for (int ii=0; ii<n; ii++)
        {

            double x = 2*(entity_vec[e2_a][ii]-entity_vec[e1_a][ii]-relation_vec[rel_a][ii]);
            if (L1_flag)
            	if (x>0)
            		x=1;
            	else
            		x=-1;
            relation_tmp[rel_a][ii]-=-1*rate*x;   // 梯度下降，修改模型参数
            entity_tmp[e1_a][ii]-=-1*rate*x;
            entity_tmp[e2_a][ii]+=-1*rate*x;
            x = 2*(entity_vec[e2_b][ii]-entity_vec[e1_b][ii]-relation_vec[rel_b][ii]);
            if (L1_flag)
            	if (x>0)
            		x=1;
            	else
            		x=-1;
            relation_tmp[rel_b][ii]-=rate*x;
            entity_tmp[e1_b][ii]-=rate*x;
            entity_tmp[e2_b][ii]+=rate*x;
        }
    }
    void train_kb(int e1_a,int e2_a,int rel_a,int e1_b,int e2_b,int rel_b)
    {
        // 训练h[i],t[i].r[i], 和对应的负样本b
        double sum1 = calc_sum(e1_a,e2_a,rel_a);  // 正样本a   h,t,r
        double sum2 = calc_sum(e1_b,e2_b,rel_b);  // 负样本b   -h,-t,-r
        if (sum1+margin>sum2)
        {
        	res+=margin+sum1-sum2;
        	gradient( e1_a, e2_a, rel_a, e1_b, e2_b, rel_b);// max_margin_loss 
        }
    }
};

Train train;
void prepare()
{
    FILE* f1 = fopen("../data/entity2id.txt","r");
	FILE* f2 = fopen("../data/relation2id.txt","r");
	int x;
	while (fscanf(f1,"%s%d",buf,&x)==2)
	{
		string st=buf;
		entity2id[st]=x;   // 全局变量 {name":id}
		id2entity[x]=st;
		entity_num++;
	}
	while (fscanf(f2,"%s%d",buf,&x)==2)
	{
		string st=buf;
		relation2id[st]=x;// 全局变量 {rname":rid}
		id2relation[x]=st;
		relation_num++;
	}
    FILE* f_kb = fopen("../data/train.txt","r");
	while (fscanf(f_kb,"%s",buf)==1)
    {
        // (h,t),r，加入class中的数据中
        string s1=buf;
        fscanf(f_kb,"%s",buf);
        string s2=buf;
        fscanf(f_kb,"%s",buf);
        string s3=buf;
        if (entity2id.count(s1)==0)
        {
            cout<<"miss entity:"<<s1<<endl;
        }
        if (entity2id.count(s2)==0)
        {
            cout<<"miss entity:"<<s2<<endl;
        }
        if (relation2id.count(s3)==0)
        {
            relation2id[s3] = relation_num;
            relation_num++;
        }
        left_entity[relation2id[s3]][entity2id[s1]]++;       // left_entity[r]={h:count}   每种关系r,每个h的出现次数
        right_entity[relation2id[s3]][entity2id[s2]]++;
        train.add(entity2id[s1],entity2id[s2],relation2id[s3]);// 训练样本(h,t,r)
    }
    for (int i=0; i<relation_num; i++)
    {
    	double sum1=0,sum2=0;
    	for (map<int,int>::iterator it = left_entity[i].begin(); it!=left_entity[i].end(); it++)  // 遍历每个r的h
    	{
    		sum1++;                    // 该r下，unique  (h,r)的数目
    		sum2+=it->second;          // 该r下，       (h,r)的总数目
    	}
    	left_num[i]=sum2/sum1;         // 该r下，(h,r)重复率. 越大，越多重复(h,r)。说明该r下只有少数几种(h,r)，但出现多次
    }
    for (int i=0; i<relation_num; i++)
    {
    	double sum1=0,sum2=0;
    	for (map<int,int>::iterator it = right_entity[i].begin(); it!=right_entity[i].end(); it++)
    	{
    		sum1++;
    		sum2+=it->second;
    	}
    	right_num[i]=sum2/sum1;
    }
    cout<<"relation_num="<<relation_num<<endl;
    cout<<"entity_num="<<entity_num<<endl;
    fclose(f_kb);
}

int ArgPos(char *str, int argc, char **argv) {
  int a;
  for (a = 1; a < argc; a++) if (!strcmp(str, argv[a])) {
    if (a == argc - 1) {
      printf("Argument missing for %s\n", str);
      exit(1);
    }
    return a;
  }
  return -1;
}

int main(int argc,char**argv)
{
    srand((unsigned) time(NULL));
    int method = 1;
    int n = 100;
    double rate = 0.001;
    double margin = 1;
    int i;
    if ((i = ArgPos((char *)"-size", argc, argv)) > 0) n = atoi(argv[i + 1]);
    if ((i = ArgPos((char *)"-margin", argc, argv)) > 0) margin = atoi(argv[i + 1]);
    if ((i = ArgPos((char *)"-method", argc, argv)) > 0) method = atoi(argv[i + 1]);
    cout<<"size = "<<n<<endl;
    cout<<"learing rate = "<<rate<<endl;
    cout<<"margin = "<<margin<<endl;
    if (method)
        version = "bern";
    else
        version = "unif";
    cout<<"method = "<<version<<endl;
    prepare();
    train.run(n,rate,margin,method);
}


